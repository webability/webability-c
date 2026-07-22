#include "json.h"

#include <stdlib.h>
#include <string.h>

struct wa_json_value {
    wa_json_type_t type;
    union {
        int boolean;
        double number;
        char *string;
        struct {
            wa_json_value_t **items;
            size_t count;
        } array;
        struct {
            char **keys;
            wa_json_value_t **values;
            size_t count;
        } object;
    } as;
};

typedef struct {
    const char *text;
    size_t len;
    size_t pos;
    int error;
} wa_json_parser_t;

static int wa_json_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int wa_json_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int wa_json_peek(wa_json_parser_t *p) {
    if (p->pos >= p->len) {
        return -1;
    }
    return (unsigned char)p->text[p->pos];
}

static void wa_json_skip_ws(wa_json_parser_t *p) {
    while (p->pos < p->len && wa_json_is_space(p->text[p->pos])) {
        p->pos++;
    }
}

static wa_json_value_t *wa_json_alloc(wa_json_type_t type) {
    wa_json_value_t *v = (wa_json_value_t *)calloc(1, sizeof(wa_json_value_t));
    if (v != NULL) {
        v->type = type;
    }
    return v;
}

static void wa_json_utf8_append(char **buf, size_t *len, size_t *cap, unsigned int cp) {
    char tmp[4];
    int n = 0;
    if (cp < 0x80) {
        tmp[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        tmp[0] = (char)(0xC0 | (cp >> 6));
        tmp[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else {
        tmp[0] = (char)(0xE0 | (cp >> 12));
        tmp[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        tmp[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    }
    if (*len + (size_t)n + 1 > *cap) {
        *cap = (*cap + (size_t)n + 1) * 2;
        *buf = (char *)realloc(*buf, *cap);
    }
    memcpy(*buf + *len, tmp, (size_t)n);
    *len += (size_t)n;
}

static int wa_json_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parsea un string JSON (asume que p->pos apunta justo despues de la comilla
 * de apertura). Devuelve un buffer recien reservado (terminado en NUL), o
 * NULL en caso de error (marca p->error). */
static char *wa_json_parse_string_raw(wa_json_parser_t *p) {
    size_t cap = 32;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (buf == NULL) {
        p->error = 1;
        return NULL;
    }

    for (;;) {
        if (p->pos >= p->len) {
            p->error = 1;
            free(buf);
            return NULL;
        }
        char c = p->text[p->pos++];
        if (c == '"') {
            if (len + 1 > cap) {
                cap = len + 1;
                buf = (char *)realloc(buf, cap);
            }
            buf[len] = '\0';
            return buf;
        }
        if (c == '\\') {
            if (p->pos >= p->len) {
                p->error = 1;
                free(buf);
                return NULL;
            }
            char esc = p->text[p->pos++];
            char lit = 0;
            switch (esc) {
                case '"': lit = '"'; break;
                case '\\': lit = '\\'; break;
                case '/': lit = '/'; break;
                case 'b': lit = '\b'; break;
                case 'f': lit = '\f'; break;
                case 'n': lit = '\n'; break;
                case 'r': lit = '\r'; break;
                case 't': lit = '\t'; break;
                case 'u': {
                    if (p->pos + 4 > p->len) {
                        p->error = 1;
                        free(buf);
                        return NULL;
                    }
                    unsigned int cp = 0;
                    for (int i = 0; i < 4; i++) {
                        int d = wa_json_hex_digit(p->text[p->pos++]);
                        if (d < 0) {
                            p->error = 1;
                            free(buf);
                            return NULL;
                        }
                        cp = (cp << 4) | (unsigned int)d;
                    }
                    wa_json_utf8_append(&buf, &len, &cap, cp);
                    continue;
                }
                default:
                    p->error = 1;
                    free(buf);
                    return NULL;
            }
            if (len + 1 > cap) {
                cap *= 2;
                buf = (char *)realloc(buf, cap);
            }
            buf[len++] = lit;
            continue;
        }
        if (len + 1 > cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
        }
        buf[len++] = c;
    }
}

static wa_json_value_t *wa_json_parse_value(wa_json_parser_t *p);

static wa_json_value_t *wa_json_parse_object(wa_json_parser_t *p) {
    wa_json_value_t *v = wa_json_alloc(WA_JSON_OBJECT);
    if (v == NULL) {
        p->error = 1;
        return NULL;
    }
    p->pos++; /* consume '{' */
    wa_json_skip_ws(p);

    if (wa_json_peek(p) == '}') {
        p->pos++;
        return v;
    }

    size_t cap = 0;
    for (;;) {
        wa_json_skip_ws(p);
        if (wa_json_peek(p) != '"') {
            p->error = 1;
            wa_json_free(v);
            return NULL;
        }
        p->pos++; /* consume opening quote */
        char *key = wa_json_parse_string_raw(p);
        if (key == NULL) {
            wa_json_free(v);
            return NULL;
        }

        wa_json_skip_ws(p);
        if (wa_json_peek(p) != ':') {
            free(key);
            p->error = 1;
            wa_json_free(v);
            return NULL;
        }
        p->pos++; /* consume ':' */
        wa_json_skip_ws(p);

        wa_json_value_t *val = wa_json_parse_value(p);
        if (val == NULL) {
            free(key);
            wa_json_free(v);
            return NULL;
        }

        if (v->as.object.count >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            v->as.object.keys = (char **)realloc(v->as.object.keys, cap * sizeof(char *));
            v->as.object.values = (wa_json_value_t **)realloc(v->as.object.values, cap * sizeof(wa_json_value_t *));
        }
        v->as.object.keys[v->as.object.count] = key;
        v->as.object.values[v->as.object.count] = val;
        v->as.object.count++;

        wa_json_skip_ws(p);
        int c = wa_json_peek(p);
        if (c == ',') {
            p->pos++;
            continue;
        }
        if (c == '}') {
            p->pos++;
            return v;
        }
        p->error = 1;
        wa_json_free(v);
        return NULL;
    }
}

static wa_json_value_t *wa_json_parse_array(wa_json_parser_t *p) {
    wa_json_value_t *v = wa_json_alloc(WA_JSON_ARRAY);
    if (v == NULL) {
        p->error = 1;
        return NULL;
    }
    p->pos++; /* consume '[' */
    wa_json_skip_ws(p);

    if (wa_json_peek(p) == ']') {
        p->pos++;
        return v;
    }

    size_t cap = 0;
    for (;;) {
        wa_json_skip_ws(p);
        wa_json_value_t *item = wa_json_parse_value(p);
        if (item == NULL) {
            wa_json_free(v);
            return NULL;
        }
        if (v->as.array.count >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            v->as.array.items = (wa_json_value_t **)realloc(v->as.array.items, cap * sizeof(wa_json_value_t *));
        }
        v->as.array.items[v->as.array.count++] = item;

        wa_json_skip_ws(p);
        int c = wa_json_peek(p);
        if (c == ',') {
            p->pos++;
            continue;
        }
        if (c == ']') {
            p->pos++;
            return v;
        }
        p->error = 1;
        wa_json_free(v);
        return NULL;
    }
}

static wa_json_value_t *wa_json_parse_value(wa_json_parser_t *p) {
    wa_json_skip_ws(p);
    int c = wa_json_peek(p);
    if (c < 0) {
        p->error = 1;
        return NULL;
    }

    if (c == '{') {
        return wa_json_parse_object(p);
    }
    if (c == '[') {
        return wa_json_parse_array(p);
    }
    if (c == '"') {
        p->pos++;
        char *s = wa_json_parse_string_raw(p);
        if (s == NULL) {
            return NULL;
        }
        wa_json_value_t *v = wa_json_alloc(WA_JSON_STRING);
        if (v == NULL) {
            free(s);
            p->error = 1;
            return NULL;
        }
        v->as.string = s;
        return v;
    }
    if (strncmp(p->text + p->pos, "true", 4) == 0 && p->pos + 4 <= p->len) {
        p->pos += 4;
        wa_json_value_t *v = wa_json_alloc(WA_JSON_BOOL);
        if (v != NULL) v->as.boolean = 1;
        return v;
    }
    if (strncmp(p->text + p->pos, "false", 5) == 0 && p->pos + 5 <= p->len) {
        p->pos += 5;
        wa_json_value_t *v = wa_json_alloc(WA_JSON_BOOL);
        if (v != NULL) v->as.boolean = 0;
        return v;
    }
    if (strncmp(p->text + p->pos, "null", 4) == 0 && p->pos + 4 <= p->len) {
        p->pos += 4;
        return wa_json_alloc(WA_JSON_NULL);
    }
    if (c == '-' || wa_json_is_digit((char)c)) {
        size_t start = p->pos;
        if (wa_json_peek(p) == '-') {
            p->pos++;
        }
        while (p->pos < p->len && wa_json_is_digit(p->text[p->pos])) {
            p->pos++;
        }
        if (p->pos < p->len && p->text[p->pos] == '.') {
            p->pos++;
            while (p->pos < p->len && wa_json_is_digit(p->text[p->pos])) {
                p->pos++;
            }
        }
        if (p->pos < p->len && (p->text[p->pos] == 'e' || p->text[p->pos] == 'E')) {
            p->pos++;
            if (p->pos < p->len && (p->text[p->pos] == '+' || p->text[p->pos] == '-')) {
                p->pos++;
            }
            while (p->pos < p->len && wa_json_is_digit(p->text[p->pos])) {
                p->pos++;
            }
        }
        size_t numlen = p->pos - start;
        char numbuf[64];
        if (numlen >= sizeof(numbuf)) {
            p->error = 1;
            return NULL;
        }
        memcpy(numbuf, p->text + start, numlen);
        numbuf[numlen] = '\0';
        wa_json_value_t *v = wa_json_alloc(WA_JSON_NUMBER);
        if (v != NULL) v->as.number = strtod(numbuf, NULL);
        return v;
    }

    p->error = 1;
    return NULL;
}

wa_json_value_t *wa_json_parse(const char *text, size_t len) {
    if (text == NULL) {
        return NULL;
    }
    wa_json_parser_t p;
    p.text = text;
    p.len = len;
    p.pos = 0;
    p.error = 0;

    wa_json_value_t *v = wa_json_parse_value(&p);
    if (p.error) {
        wa_json_free(v);
        return NULL;
    }
    return v;
}

void wa_json_free(wa_json_value_t *value) {
    if (value == NULL) {
        return;
    }
    switch (value->type) {
        case WA_JSON_STRING:
            free(value->as.string);
            break;
        case WA_JSON_ARRAY:
            for (size_t i = 0; i < value->as.array.count; i++) {
                wa_json_free(value->as.array.items[i]);
            }
            free(value->as.array.items);
            break;
        case WA_JSON_OBJECT:
            for (size_t i = 0; i < value->as.object.count; i++) {
                free(value->as.object.keys[i]);
                wa_json_free(value->as.object.values[i]);
            }
            free(value->as.object.keys);
            free(value->as.object.values);
            break;
        default:
            break;
    }
    free(value);
}

wa_json_type_t wa_json_type(const wa_json_value_t *value) {
    if (value == NULL) {
        return WA_JSON_NULL;
    }
    return value->type;
}

wa_json_value_t *wa_json_object_get(const wa_json_value_t *value, const char *key) {
    if (value == NULL || value->type != WA_JSON_OBJECT) {
        return NULL;
    }
    for (size_t i = 0; i < value->as.object.count; i++) {
        if (strcmp(value->as.object.keys[i], key) == 0) {
            return value->as.object.values[i];
        }
    }
    return NULL;
}

size_t wa_json_array_length(const wa_json_value_t *value) {
    if (value == NULL || value->type != WA_JSON_ARRAY) {
        return 0;
    }
    return value->as.array.count;
}

wa_json_value_t *wa_json_array_get(const wa_json_value_t *value, size_t index) {
    if (value == NULL || value->type != WA_JSON_ARRAY || index >= value->as.array.count) {
        return NULL;
    }
    return value->as.array.items[index];
}

const char *wa_json_as_string(const wa_json_value_t *value, const char *default_value) {
    if (value == NULL || value->type != WA_JSON_STRING) {
        return default_value;
    }
    return value->as.string;
}

long wa_json_as_int(const wa_json_value_t *value, long default_value) {
    if (value == NULL || value->type != WA_JSON_NUMBER) {
        return default_value;
    }
    return (long)value->as.number;
}

const char *wa_json_get_string(const wa_json_value_t *value, const char *key, const char *default_value) {
    return wa_json_as_string(wa_json_object_get(value, key), default_value);
}

long wa_json_get_int(const wa_json_value_t *value, const char *key, long default_value) {
    return wa_json_as_int(wa_json_object_get(value, key), default_value);
}
