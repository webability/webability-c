/*
 * mail.h
 *
 * En construccion — calca el contrato del SDK de Go
 * (github.com/webability/webability-go/mail).
 *
 * Pendiente de implementar: este modulo depende de la capa de transporte
 * HTTP (firma HMAC-SHA256 + requests reales), que webability_api.h todavia
 * no implementa. Las firmas ya estan fijadas para que la implementacion
 * futura sea un port directo de mail.go, no un rediseno.
 */

#ifndef WEBABILITY_MAIL_H
#define WEBABILITY_MAIL_H

#include "webability_api.h"

/* Estados posibles de queue_status. */
#define WA_QUEUE_STATUS_PENDING    "pending"
#define WA_QUEUE_STATUS_PROCESSING "processing"
#define WA_QUEUE_STATUS_SENT       "sent"
#define WA_QUEUE_STATUS_ERROR      "error"

typedef struct {
    const char *email;
    const char *name;
} wa_mail_address_t;

typedef struct {
    const char *email;
    const char *name;
    /* vars: pendiente de definir (lista clave/valor) cuando se implemente. */
} wa_mail_recipient_t;

/* Campos para POST /v1/mail/send. */
typedef struct {
    wa_mail_address_t from;
    wa_mail_recipient_t to;
    /* template: si no es NULL/"", es el id de una plantilla ya registrada y
     * activa en templates_template bajo la cuenta que autentica el request —
     * el servidor arma el correo con esa plantilla en vez de
     * subject/html/text (que se ignoran si template viene). La
     * personalizacion usa las vars de "to", igual que en el envio ad-hoc. El
     * servidor valida que la plantilla exista y este activa ANTES de
     * encolar el correo: si no, wa_mail_send() devuelve 1 (error de API,
     * codigos 3025/3026), no un envio "pending" fallido. */
    const char *template;
    const char *subject;
    const char *html;
    const char *text;
    int track_opens;
    int track_clicks;
    /* wait_send: si es 1, el servidor espera (hasta ~20s) el resultado real
     * del envio antes de responder, en vez de responder de inmediato con
     * queue_status="pending". Ver wa_mail_send(). */
    int wait_send;
} wa_mail_send_request_t;

/* Respuesta de wa_mail_send(). */
typedef struct {
    const char *status;
    long queue_key;
    const char *queue_status;
    const char *error_detail; /* NULL si no hay error */
    const char *to;
} wa_mail_send_result_t;

/* Respuesta de wa_mail_status(). */
typedef struct {
    const char *status;
    long queue_key;
    const char *queue_status;
    const char *error_detail; /* NULL si no hay error */
} wa_mail_status_result_t;

/* Envia un correo a un solo destinatario. POST /v1/mail/send
 *
 * EN CONSTRUCCION: siempre devuelve -1 (no implementado). */
int wa_mail_send(const wa_api_t *api, const wa_mail_send_request_t *req, wa_mail_send_result_t *out);

/* Consulta el estatus real de un envio hecho con wa_mail_send().
 * GET /v1/mail/status/{queue_key}
 *
 * EN CONSTRUCCION: siempre devuelve -1 (no implementado). */
int wa_mail_status(const wa_api_t *api, long queue_key, wa_mail_status_result_t *out);

#endif /* WEBABILITY_MAIL_H */
