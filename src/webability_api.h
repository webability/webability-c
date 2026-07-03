/*
 * webability_api.h
 *
 * En construccion.
 *
 * Cliente base en C para la API de WebAbility (https://api.webability.info).
 * Seguira el mismo esquema de autenticacion que el cliente Go de referencia
 * (github.com/webability/webability-go): ClientID + Token, firma HMAC-SHA256 en los
 * headers X-WA-Client / X-WA-Timestamp / X-WA-Digest. El Token nunca viaja
 * en el request.
 */

#ifndef WEBABILITY_API_H
#define WEBABILITY_API_H

typedef struct {
    const char *client_id;
    const char *token;
    const char *base_url;
} wa_api_t;

wa_api_t wa_api_new(const char *client_id, const char *token);

#endif /* WEBABILITY_API_H */
