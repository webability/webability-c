#include "dns.h"
#include "json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *wa_strdup(const char *s) {
    if (s == NULL) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

/* ── Constructor de strings para armar el JSON de salida ──────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} wa_strbuf_t;

static void wa_strbuf_init(wa_strbuf_t *b) {
    b->cap = 256;
    b->data = (char *)malloc(b->cap);
    b->len = 0;
    if (b->data != NULL) {
        b->data[0] = '\0';
    }
}

static void wa_strbuf_append(wa_strbuf_t *b, const char *s) {
    size_t addlen = strlen(s);
    if (b->len + addlen + 1 > b->cap) {
        size_t newcap = b->cap == 0 ? 256 : b->cap;
        while (newcap < b->len + addlen + 1) {
            newcap *= 2;
        }
        b->data = (char *)realloc(b->data, newcap);
        b->cap = newcap;
    }
    memcpy(b->data + b->len, s, addlen + 1); /* incluye el NUL */
    b->len += addlen;
}

static void wa_strbuf_append_fmt(wa_strbuf_t *b, const char *fmt, ...) {
    char tmp[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    wa_strbuf_append(b, tmp);
}

static void wa_dns_add_comma_if_needed(wa_strbuf_t *b, int *first) {
    if (!*first) {
        wa_strbuf_append(b, ",");
    }
    *first = 0;
}

/* Escapa un string para insertarlo como valor JSON entre comillas.
 * Devuelve un buffer nuevo (el llamador debe liberarlo con free()). */
static char *wa_dns_json_escape(const char *s) {
    if (s == NULL) {
        s = "";
    }
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 6 + 1); /* peor caso: \u00XX por char */
    if (out == NULL) {
        return NULL;
    }
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':
                out[o++] = '\\';
                out[o++] = '"';
                break;
            case '\\':
                out[o++] = '\\';
                out[o++] = '\\';
                break;
            case '\n':
                out[o++] = '\\';
                out[o++] = 'n';
                break;
            case '\r':
                out[o++] = '\\';
                out[o++] = 'r';
                break;
            case '\t':
                out[o++] = '\\';
                out[o++] = 't';
                break;
            default:
                if (c < 0x20) {
                    o += (size_t)snprintf(out + o, 7, "\\u%04x", c);
                } else {
                    out[o++] = (char)c;
                }
        }
    }
    out[o] = '\0';
    return out;
}

/* ── Parseo de respuestas ──────────────────────────────────────────────── */

static void wa_dns_zone_from_json(const wa_json_value_t *j, wa_dns_zone_t *zone) {
    zone->key = wa_json_get_int(j, "key", 0);
    zone->name = wa_strdup(wa_json_get_string(j, "name", ""));
    zone->status = wa_json_get_int(j, "status", 0);
    zone->primaryns = wa_strdup(wa_json_get_string(j, "primaryns", ""));
    zone->adminemail = wa_strdup(wa_json_get_string(j, "adminemail", ""));
    zone->serial = wa_json_get_int(j, "serial", 0);
    zone->refresh = wa_json_get_int(j, "refresh", 0);
    zone->retry = wa_json_get_int(j, "retry", 0);
    zone->expire = wa_json_get_int(j, "expire", 0);
    zone->minimum = wa_json_get_int(j, "minimum", 0);
    zone->defaultttl = wa_json_get_int(j, "defaultttl", 0);
    zone->dnssec = wa_json_get_int(j, "dnssec", 0);
    zone->creationdate = wa_strdup(wa_json_get_string(j, "creationdate", ""));
}

static void wa_dns_record_from_json(const wa_json_value_t *j, wa_dns_record_t *rec) {
    rec->key = wa_json_get_int(j, "key", 0);
    rec->zone = wa_json_get_int(j, "zone", 0);
    rec->name = wa_strdup(wa_json_get_string(j, "name", ""));
    rec->rrtype = wa_json_get_int(j, "rrtype", 0);
    rec->rrtypename = wa_strdup(wa_json_get_string(j, "rrtypename", ""));
    rec->ttl = wa_json_get_int(j, "ttl", 0);
    rec->status = wa_json_get_int(j, "status", 0);
    rec->priority = wa_json_get_int(j, "priority", 0);
    rec->weight = wa_json_get_int(j, "weight", 0);
    rec->port = wa_json_get_int(j, "port", 0);
    rec->tag = wa_strdup(wa_json_get_string(j, "tag", ""));
    rec->data = wa_strdup(wa_json_get_string(j, "data", ""));
}

static char *wa_dns_extract_error(const wa_response_t *resp) {
    wa_json_value_t *root = wa_json_parse(resp->body, resp->body_len);
    const char *msg = wa_json_get_string(root, "message", NULL);
    char *result = wa_strdup(msg != NULL ? msg : "error desconocido de la API");
    wa_json_free(root);
    return result;
}

/* Percent-encoding mínimo para un segmento de path (dominios y claves
 * numéricas no necesitan escapar nada, pero por si acaso). */
static char *wa_dns_urlencode(const char *s) {
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 3 + 1);
    if (out == NULL) {
        return NULL;
    }
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '_' || c == '~') {
            out[o++] = (char)c;
        } else {
            o += (size_t)snprintf(out + o, 4, "%%%02X", c);
        }
    }
    out[o] = '\0';
    return out;
}

/* ── API pública ───────────────────────────────────────────────────────── */

wa_dns_t wa_dns_new(wa_api_t *api) {
    wa_dns_t dns;
    dns.api = api;
    return dns;
}

int wa_dns_list_zones(const wa_dns_t *dns, wa_dns_zone_list_t *out, char **out_error) {
    wa_response_t resp;
    int rc = wa_api_get(dns->api, "/v1/dns/zone", &resp);
    if (rc < 0) {
        if (out_error) *out_error = wa_strdup("error de transporte");
        return -1;
    }
    if (rc > 0) {
        if (out_error) *out_error = wa_dns_extract_error(&resp);
        wa_response_free(&resp);
        return 1;
    }

    wa_json_value_t *root = wa_json_parse(resp.body, resp.body_len);
    wa_response_free(&resp);
    if (root == NULL) {
        if (out_error) *out_error = wa_strdup("respuesta JSON inválida");
        return -1;
    }

    wa_json_value_t *zones = wa_json_object_get(root, "zones");
    size_t n = wa_json_array_length(zones);
    out->zones = n > 0 ? (wa_dns_zone_t *)calloc(n, sizeof(wa_dns_zone_t)) : NULL;
    out->count = n;
    for (size_t i = 0; i < n; i++) {
        wa_dns_zone_from_json(wa_json_array_get(zones, i), &out->zones[i]);
    }
    wa_json_free(root);
    return 0;
}

int wa_dns_get_zone(const wa_dns_t *dns, const char *key_or_domain, wa_dns_zone_detail_t *out, char **out_error) {
    char *encoded = wa_dns_urlencode(key_or_domain);
    size_t path_len = strlen("/v1/dns/zone/") + strlen(encoded) + 1;
    char *path = (char *)malloc(path_len);
    snprintf(path, path_len, "/v1/dns/zone/%s", encoded);
    free(encoded);

    wa_response_t resp;
    int rc = wa_api_get(dns->api, path, &resp);
    free(path);
    if (rc < 0) {
        if (out_error) *out_error = wa_strdup("error de transporte");
        return -1;
    }
    if (rc > 0) {
        if (out_error) *out_error = wa_dns_extract_error(&resp);
        wa_response_free(&resp);
        return 1;
    }

    wa_json_value_t *root = wa_json_parse(resp.body, resp.body_len);
    wa_response_free(&resp);
    if (root == NULL) {
        if (out_error) *out_error = wa_strdup("respuesta JSON inválida");
        return -1;
    }

    wa_dns_zone_from_json(wa_json_object_get(root, "zone"), &out->zone);

    wa_json_value_t *records_j = wa_json_object_get(root, "records");
    size_t rn = wa_json_array_length(records_j);
    out->records = rn > 0 ? (wa_dns_record_t *)calloc(rn, sizeof(wa_dns_record_t)) : NULL;
    out->records_count = rn;
    for (size_t i = 0; i < rn; i++) {
        wa_dns_record_from_json(wa_json_array_get(records_j, i), &out->records[i]);
    }

    wa_json_value_t *ns_j = wa_json_object_get(root, "ns");
    size_t nsn = wa_json_array_length(ns_j);
    out->ns = nsn > 0 ? (char **)calloc(nsn, sizeof(char *)) : NULL;
    out->ns_count = nsn;
    for (size_t i = 0; i < nsn; i++) {
        out->ns[i] = wa_strdup(wa_json_as_string(wa_json_array_get(ns_j, i), ""));
    }

    wa_json_free(root);
    return 0;
}

int wa_dns_add_zone(const wa_dns_t *dns, const char *name, long *out_key, char **out_error) {
    char *esc = wa_dns_json_escape(name);
    wa_strbuf_t b;
    wa_strbuf_init(&b);
    wa_strbuf_append(&b, "{\"name\":\"");
    wa_strbuf_append(&b, esc);
    wa_strbuf_append(&b, "\"}");
    free(esc);

    wa_response_t resp;
    int rc = wa_api_post(dns->api, "/v1/dns/zone", b.data, &resp);
    free(b.data);
    if (rc < 0) {
        if (out_error) *out_error = wa_strdup("error de transporte");
        return -1;
    }
    if (rc > 0) {
        if (out_error) *out_error = wa_dns_extract_error(&resp);
        wa_response_free(&resp);
        return 1;
    }

    wa_json_value_t *root = wa_json_parse(resp.body, resp.body_len);
    wa_response_free(&resp);
    if (root == NULL) {
        if (out_error) *out_error = wa_strdup("respuesta JSON inválida");
        return -1;
    }
    if (out_key) {
        *out_key = wa_json_get_int(root, "key", 0);
    }
    wa_json_free(root);
    return 0;
}

int wa_dns_add_record(const wa_dns_t *dns, long zone_key, const wa_dns_record_input_t *record,
                       long *out_key, char **out_error) {
    char path[64];
    snprintf(path, sizeof(path), "/v1/dns/zone/%ld/record", zone_key);

    char *esc_name = wa_dns_json_escape(record->name);
    char *esc_rrtype = wa_dns_json_escape(record->rrtype);
    char *esc_data = wa_dns_json_escape(record->data);
    char *esc_tag = wa_dns_json_escape(record->tag != NULL ? record->tag : "");

    wa_strbuf_t b;
    wa_strbuf_init(&b);
    wa_strbuf_append(&b, "{\"name\":\"");
    wa_strbuf_append(&b, esc_name);
    wa_strbuf_append(&b, "\",\"rrtype\":\"");
    wa_strbuf_append(&b, esc_rrtype);
    wa_strbuf_append_fmt(&b, "\",\"ttl\":%ld,\"data\":\"", record->ttl);
    wa_strbuf_append(&b, esc_data);
    wa_strbuf_append_fmt(&b, "\",\"priority\":%ld,\"weight\":%ld,\"port\":%ld,\"tag\":\"",
                         record->priority, record->weight, record->port);
    wa_strbuf_append(&b, esc_tag);
    wa_strbuf_append(&b, "\"}");

    free(esc_name);
    free(esc_rrtype);
    free(esc_data);
    free(esc_tag);

    wa_response_t resp;
    int rc = wa_api_post(dns->api, path, b.data, &resp);
    free(b.data);
    if (rc < 0) {
        if (out_error) *out_error = wa_strdup("error de transporte");
        return -1;
    }
    if (rc > 0) {
        if (out_error) *out_error = wa_dns_extract_error(&resp);
        wa_response_free(&resp);
        return 1;
    }

    wa_json_value_t *root = wa_json_parse(resp.body, resp.body_len);
    wa_response_free(&resp);
    if (root == NULL) {
        if (out_error) *out_error = wa_strdup("respuesta JSON inválida");
        return -1;
    }
    if (out_key) {
        *out_key = wa_json_get_int(root, "key", 0);
    }
    wa_json_free(root);
    return 0;
}

static char *wa_dns_build_update_body(const wa_dns_record_update_t *fields) {
    wa_strbuf_t b;
    wa_strbuf_init(&b);
    wa_strbuf_append(&b, "{");
    int first = 1;

    if (fields->has_name) {
        wa_dns_add_comma_if_needed(&b, &first);
        char *esc = wa_dns_json_escape(fields->name);
        wa_strbuf_append(&b, "\"name\":\"");
        wa_strbuf_append(&b, esc);
        wa_strbuf_append(&b, "\"");
        free(esc);
    }
    if (fields->has_ttl) {
        wa_dns_add_comma_if_needed(&b, &first);
        wa_strbuf_append_fmt(&b, "\"ttl\":%ld", fields->ttl);
    }
    if (fields->has_data) {
        wa_dns_add_comma_if_needed(&b, &first);
        char *esc = wa_dns_json_escape(fields->data);
        wa_strbuf_append(&b, "\"data\":\"");
        wa_strbuf_append(&b, esc);
        wa_strbuf_append(&b, "\"");
        free(esc);
    }
    if (fields->has_priority) {
        wa_dns_add_comma_if_needed(&b, &first);
        wa_strbuf_append_fmt(&b, "\"priority\":%ld", fields->priority);
    }
    if (fields->has_weight) {
        wa_dns_add_comma_if_needed(&b, &first);
        wa_strbuf_append_fmt(&b, "\"weight\":%ld", fields->weight);
    }
    if (fields->has_port) {
        wa_dns_add_comma_if_needed(&b, &first);
        wa_strbuf_append_fmt(&b, "\"port\":%ld", fields->port);
    }
    if (fields->has_tag) {
        wa_dns_add_comma_if_needed(&b, &first);
        char *esc = wa_dns_json_escape(fields->tag);
        wa_strbuf_append(&b, "\"tag\":\"");
        wa_strbuf_append(&b, esc);
        wa_strbuf_append(&b, "\"");
        free(esc);
    }
    if (fields->has_status) {
        wa_dns_add_comma_if_needed(&b, &first);
        wa_strbuf_append_fmt(&b, "\"status\":%ld", fields->status);
    }

    wa_strbuf_append(&b, "}");
    return b.data;
}

int wa_dns_update_record(const wa_dns_t *dns, long record_key, const wa_dns_record_update_t *fields,
                          char **out_error) {
    char path[64];
    snprintf(path, sizeof(path), "/v1/dns/record/%ld", record_key);

    char *body = wa_dns_build_update_body(fields);

    wa_response_t resp;
    int rc = wa_api_put(dns->api, path, body, &resp);
    free(body);
    if (rc < 0) {
        if (out_error) *out_error = wa_strdup("error de transporte");
        return -1;
    }
    if (rc > 0) {
        if (out_error) *out_error = wa_dns_extract_error(&resp);
        wa_response_free(&resp);
        return 1;
    }
    wa_response_free(&resp);
    return 0;
}

int wa_dns_delete_record(const wa_dns_t *dns, long record_key, char **out_error) {
    char path[64];
    snprintf(path, sizeof(path), "/v1/dns/record/%ld", record_key);

    wa_response_t resp;
    int rc = wa_api_delete(dns->api, path, &resp);
    if (rc < 0) {
        if (out_error) *out_error = wa_strdup("error de transporte");
        return -1;
    }
    if (rc > 0) {
        if (out_error) *out_error = wa_dns_extract_error(&resp);
        wa_response_free(&resp);
        return 1;
    }
    wa_response_free(&resp);
    return 0;
}

int wa_dns_delete_zone(const wa_dns_t *dns, long zone_key, char **out_error) {
    char path[64];
    snprintf(path, sizeof(path), "/v1/dns/zone/%ld", zone_key);

    wa_response_t resp;
    int rc = wa_api_delete(dns->api, path, &resp);
    if (rc < 0) {
        if (out_error) *out_error = wa_strdup("error de transporte");
        return -1;
    }
    if (rc > 0) {
        if (out_error) *out_error = wa_dns_extract_error(&resp);
        wa_response_free(&resp);
        return 1;
    }
    wa_response_free(&resp);
    return 0;
}

/* ── Liberación de memoria ─────────────────────────────────────────────── */

void wa_dns_zone_free(wa_dns_zone_t *zone) {
    if (zone == NULL) {
        return;
    }
    free(zone->name);
    free(zone->primaryns);
    free(zone->adminemail);
    free(zone->creationdate);
}

void wa_dns_record_free(wa_dns_record_t *record) {
    if (record == NULL) {
        return;
    }
    free(record->name);
    free(record->rrtypename);
    free(record->tag);
    free(record->data);
}

void wa_dns_zone_list_free(wa_dns_zone_list_t *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        wa_dns_zone_free(&list->zones[i]);
    }
    free(list->zones);
    list->zones = NULL;
    list->count = 0;
}

void wa_dns_zone_detail_free(wa_dns_zone_detail_t *detail) {
    if (detail == NULL) {
        return;
    }
    wa_dns_zone_free(&detail->zone);
    for (size_t i = 0; i < detail->records_count; i++) {
        wa_dns_record_free(&detail->records[i]);
    }
    free(detail->records);
    detail->records = NULL;
    detail->records_count = 0;
    for (size_t i = 0; i < detail->ns_count; i++) {
        free(detail->ns[i]);
    }
    free(detail->ns);
    detail->ns = NULL;
    detail->ns_count = 0;
}
