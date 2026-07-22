#include "webability_api.h"

#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

wa_api_t wa_api_new(const char *client_id, const char *token) {
    return wa_api_new_with_url(WA_DEFAULT_BASE_URL, client_id, token);
}

wa_api_t wa_api_new_with_url(const char *base_url, const char *client_id, const char *token) {
    wa_api_t api;
    api.client_id = wa_strdup(client_id);
    api.token = wa_strdup(token);
    api.base_url = wa_strdup(base_url);
    return api;
}

void wa_api_free(wa_api_t *api) {
    if (api == NULL) {
        return;
    }
    free(api->client_id);
    free(api->token);
    free(api->base_url);
    api->client_id = NULL;
    api->token = NULL;
    api->base_url = NULL;
}

void wa_response_free(wa_response_t *response) {
    if (response == NULL) {
        return;
    }
    free(response->body);
    response->body = NULL;
    response->body_len = 0;
}

char *wa_api_digest(const wa_api_t *api, const char *message) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;

    unsigned char *result = HMAC(EVP_sha256(),
                                  api->token, (int)strlen(api->token),
                                  (const unsigned char *)message, strlen(message),
                                  md, &md_len);
    if (result == NULL) {
        return NULL;
    }

    char *hex = (char *)malloc((size_t)md_len * 2 + 1);
    if (hex == NULL) {
        return NULL;
    }
    for (unsigned int i = 0; i < md_len; i++) {
        snprintf(hex + i * 2, 3, "%02x", md[i]);
    }
    return hex;
}

/* Buffer dinámico usado por el callback de escritura de libcurl. */
struct wa_curl_buffer {
    char *data;
    size_t len;
    size_t cap;
};

static size_t wa_curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    struct wa_curl_buffer *buf = (struct wa_curl_buffer *)userdata;
    size_t add = size * nmemb;

    if (buf->len + add + 1 > buf->cap) {
        size_t newcap = buf->cap == 0 ? 1024 : buf->cap;
        while (newcap < buf->len + add + 1) {
            newcap *= 2;
        }
        char *newdata = (char *)realloc(buf->data, newcap);
        if (newdata == NULL) {
            return 0; /* señala error a libcurl */
        }
        buf->data = newdata;
        buf->cap = newcap;
    }

    memcpy(buf->data + buf->len, ptr, add);
    buf->len += add;
    buf->data[buf->len] = '\0';
    return add;
}

int wa_api_request(const wa_api_t *api, const char *method, const char *path,
                    const char *body_json, wa_response_t *out) {
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%ld", (long)time(NULL));

    size_t msg_len = strlen(method) + strlen(path) + strlen(timestamp) + strlen(api->client_id) + 4;
    char *message = (char *)malloc(msg_len);
    if (message == NULL) {
        return -1;
    }
    snprintf(message, msg_len, "%s|%s|%s|%s", method, path, timestamp, api->client_id);

    char *digest = wa_api_digest(api, message);
    free(message);
    if (digest == NULL) {
        return -1;
    }

    size_t url_len = strlen(api->base_url) + strlen(path) + 1;
    char *url = (char *)malloc(url_len);
    if (url == NULL) {
        free(digest);
        return -1;
    }
    snprintf(url, url_len, "%s%s", api->base_url, path);

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        free(digest);
        free(url);
        return -1;
    }

    char header_client[300];
    char header_ts[64];
    char header_digest[128];
    snprintf(header_client, sizeof(header_client), "X-WA-Client: %s", api->client_id);
    snprintf(header_ts, sizeof(header_ts), "X-WA-Timestamp: %s", timestamp);
    snprintf(header_digest, sizeof(header_digest), "X-WA-Digest: %s", digest);
    free(digest);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, header_client);
    headers = curl_slist_append(headers, header_ts);
    headers = curl_slist_append(headers, header_digest);
    if (body_json != NULL) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }

    struct wa_curl_buffer buf;
    buf.data = NULL;
    buf.len = 0;
    buf.cap = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wa_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    if (body_json != NULL) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_json);
    }

    CURLcode res = curl_easy_perform(curl);
    free(url);

    if (res != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(buf.data);
        return -1;
    }

    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    out->status_code = status_code;
    out->body = buf.data;
    out->body_len = buf.len;

    return status_code >= 400 ? 1 : 0;
}

int wa_api_get(const wa_api_t *api, const char *path, wa_response_t *out) {
    return wa_api_request(api, "GET", path, NULL, out);
}

int wa_api_post(const wa_api_t *api, const char *path, const char *body_json, wa_response_t *out) {
    return wa_api_request(api, "POST", path, body_json != NULL ? body_json : "{}", out);
}

int wa_api_put(const wa_api_t *api, const char *path, const char *body_json, wa_response_t *out) {
    return wa_api_request(api, "PUT", path, body_json != NULL ? body_json : "{}", out);
}

int wa_api_delete(const wa_api_t *api, const char *path, wa_response_t *out) {
    return wa_api_request(api, "DELETE", path, NULL, out);
}
