/*
 * json.h — parser JSON minimo, de solo lectura, sin dependencias.
 *
 * Suficiente para decodificar las respuestas de la API de WebAbility
 * (objetos, arrays, strings, numeros, booleanos, null). No es un parser JSON
 * de proposito general con todas las validaciones del estandar (no rechaza
 * todo JSON invalido), pero acepta cualquier JSON valido que la API genere.
 *
 * Uso tipico:
 *
 *   wa_json_value_t *root = wa_json_parse(body, body_len);
 *   long key = wa_json_get_int(root, "key", 0);
 *   const char *name = wa_json_get_string(root, "name", "");
 *   wa_json_value_t *zones = wa_json_object_get(root, "zones");
 *   size_t n = wa_json_array_length(zones);
 *   for (size_t i = 0; i < n; i++) {
 *       wa_json_value_t *zone = wa_json_array_get(zones, i);
 *       ...
 *   }
 *   wa_json_free(root);
 */

#ifndef WEBABILITY_JSON_H
#define WEBABILITY_JSON_H

#include <stddef.h>

typedef enum {
    WA_JSON_NULL,
    WA_JSON_BOOL,
    WA_JSON_NUMBER,
    WA_JSON_STRING,
    WA_JSON_ARRAY,
    WA_JSON_OBJECT,
} wa_json_type_t;

typedef struct wa_json_value wa_json_value_t;

/* Parsea text (de longitud len) y devuelve el valor raiz, o NULL si el JSON
 * es invalido. El valor devuelto debe liberarse con wa_json_free(). */
wa_json_value_t *wa_json_parse(const char *text, size_t len);

/* Libera un valor y todos sus hijos (recursivamente). Acepta NULL. */
void wa_json_free(wa_json_value_t *value);

wa_json_type_t wa_json_type(const wa_json_value_t *value);

/* Busca "key" en un objeto. Devuelve NULL si value no es un objeto o la
 * clave no existe. */
wa_json_value_t *wa_json_object_get(const wa_json_value_t *value, const char *key);

/* Longitud de un array. Devuelve 0 si value no es un array. */
size_t wa_json_array_length(const wa_json_value_t *value);

/* Elemento index de un array. Devuelve NULL si value no es un array o index
 * está fuera de rango. */
wa_json_value_t *wa_json_array_get(const wa_json_value_t *value, size_t index);

/* Atajos: buscan "key" en un objeto y devuelven el valor ya convertido,
 * o default_value si la clave no existe / value no es un objeto / el tipo
 * no coincide. La cadena devuelta por wa_json_get_string apunta a memoria
 * interna del valor — válida mientras el wa_json_value_t no se libere. */
const char *wa_json_get_string(const wa_json_value_t *value, const char *key, const char *default_value);
long wa_json_get_int(const wa_json_value_t *value, const char *key, long default_value);

/* Extrae el string/numero de un valor directamente (no de un objeto). */
const char *wa_json_as_string(const wa_json_value_t *value, const char *default_value);
long wa_json_as_int(const wa_json_value_t *value, long default_value);

#endif /* WEBABILITY_JSON_H */
