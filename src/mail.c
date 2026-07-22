#include "mail.h"

int wa_mail_send(const wa_api_t *api, const wa_mail_send_request_t *req, wa_mail_send_result_t *out) {
    (void)api;
    (void)req;
    (void)out;
    return -1; /* no implementado */
}

int wa_mail_status(const wa_api_t *api, long queue_key, wa_mail_status_result_t *out) {
    (void)api;
    (void)queue_key;
    (void)out;
    return -1; /* no implementado */
}
