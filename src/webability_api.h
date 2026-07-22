/*
 * webability_api.h
 *
 * Cliente base en C para la API de WebAbility (https://api.webability.info).
 *
 * Firma cada request con HMAC-SHA256 (headers X-WA-Client, X-WA-Timestamp,
 * X-WA-Digest) — mismo esquema que el SDK de Go
 * (github.com/webability/webability-go/wa). El Token nunca viaja en el
 * request: solo se usa localmente para calcular el digest.
 *
 * Dependencias del sistema: libcurl (transporte HTTP) y OpenSSL (HMAC-SHA256).
 * En Linux/macOS normalmente ya están instaladas; si no, instala los
 * paquetes de desarrollo (ej. Debian/Ubuntu: libcurl4-openssl-dev,
 * libssl-dev) y enlaza con -lcurl -lcrypto (ver Makefile).
 */

#ifndef WEBABILITY_API_H
#define WEBABILITY_API_H

#include <stddef.h>

#define WA_DEFAULT_BASE_URL "https://api.webability.info"

typedef struct {
    char *client_id;
    char *token;
    char *base_url;
} wa_api_t;

/* Respuesta cruda de un request a la API. body es un buffer propio,
 * terminado en NUL (body puede ser NULL si el servidor no devolvió cuerpo).
 * Libera con wa_response_free(). */
typedef struct {
    long status_code;
    char *body;
    size_t body_len;
} wa_response_t;

/* Crea un wa_api_t con el host por defecto. client_id y token se copian
 * internamente — el llamador conserva la propiedad de los punteros
 * originales y puede liberarlos/reusarlos después de esta llamada. */
wa_api_t wa_api_new(const char *client_id, const char *token);

/* Igual que wa_api_new pero con un host alternativo (útil para pruebas). */
wa_api_t wa_api_new_with_url(const char *base_url, const char *client_id, const char *token);

/* Libera la memoria interna de un wa_api_t (client_id/token/base_url). No
 * libera el propio struct (normalmente se usa por valor, en el stack). */
void wa_api_free(wa_api_t *api);

/* Retorna hex(HMAC-SHA256(api->token, message)) en un buffer recién
 * reservado que el llamador debe liberar con free(). Devuelve NULL en caso
 * de error. */
char *wa_api_digest(const wa_api_t *api, const char *message);

/*
 * Firma y envía un request HTTP a la API.
 *
 * path debe ser la ruta absoluta (ej: "/v1/dns/zone"), sin el host y sin
 * query string. body_json, si no es NULL, se envía tal cual como cuerpo del
 * request con Content-Type: application/json — el llamador es responsable
 * de construir el JSON (ver dns.c para ejemplos con snprintf).
 *
 * Devuelve:
 *   0  éxito (status HTTP < 400); *out queda lleno.
 *   1  la API respondió con status >= 400; *out queda lleno con el body de
 *      error crudo — usa wa_json_parse() sobre out->body para leer
 *      "code"/"message".
 *  -1  error de transporte (no se pudo completar el request); *out no es
 *      válido.
 */
int wa_api_request(const wa_api_t *api, const char *method, const char *path,
                    const char *body_json, wa_response_t *out);

int wa_api_get(const wa_api_t *api, const char *path, wa_response_t *out);
int wa_api_post(const wa_api_t *api, const char *path, const char *body_json, wa_response_t *out);
int wa_api_put(const wa_api_t *api, const char *path, const char *body_json, wa_response_t *out);
int wa_api_delete(const wa_api_t *api, const char *path, wa_response_t *out);

/* Libera el body de una wa_response_t (no libera el propio struct). */
void wa_response_free(wa_response_t *response);

#endif /* WEBABILITY_API_H */
