#include "webability_api.h"

wa_api_t wa_api_new(const char *client_id, const char *token) {
    wa_api_t api;
    api.client_id = client_id;
    api.token = token;
    api.base_url = "https://api.webability.info";
    return api;
}
