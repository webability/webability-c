/*
 * dns.h — módulo DNS: zonas y registros del cliente. Envuelve /v1/dns/*.
 */

#ifndef WEBABILITY_DNS_H
#define WEBABILITY_DNS_H

#include <stddef.h>

#include "webability_api.h"

typedef struct {
    long key;
    char *name;
    long status;
    char *primaryns;
    char *adminemail;
    long serial;
    long refresh;
    long retry;
    long expire;
    long minimum;
    long defaultttl;
    long dnssec;
    char *creationdate;
} wa_dns_zone_t;

typedef struct {
    long key;
    long zone;
    char *name;
    long rrtype;
    char *rrtypename;
    long ttl;
    long status;
    long priority;
    long weight;
    long port;
    char *tag;
    char *data;
} wa_dns_record_t;

typedef struct {
    wa_dns_zone_t *zones;
    size_t count;
} wa_dns_zone_list_t;

typedef struct {
    wa_dns_zone_t zone;
    wa_dns_record_t *records;
    size_t records_count;
    char **ns;
    size_t ns_count;
} wa_dns_zone_detail_t;

/* Campos para crear un registro con wa_dns_add_record. priority/weight/port
 * en 0 y tag en NULL/"" equivalen a "no aplica" para ese tipo de registro. */
typedef struct {
    const char *name;
    const char *rrtype;
    long ttl;
    const char *data;
    long priority;
    long weight;
    long port;
    const char *tag;
} wa_dns_record_input_t;

/* Campos opcionales para modificar un registro con wa_dns_update_record.
 * Los has_* indican si ese campo debe enviarse — a diferencia de
 * wa_dns_record_input_t, aquí sí importa distinguir "no tocar" de "poner en
 * 0/cadena vacía", así que cada campo tiene su propio flag. */
typedef struct {
    int has_name;
    const char *name;
    int has_ttl;
    long ttl;
    int has_data;
    const char *data;
    int has_priority;
    long priority;
    int has_weight;
    long weight;
    int has_port;
    long port;
    int has_tag;
    const char *tag;
    int has_status;
    long status;
} wa_dns_record_update_t;

typedef struct {
    wa_api_t *api;
} wa_dns_t;

/* Enlaza un wa_api_t (ya inicializado con wa_api_new) para hacer las
 * llamadas al servicio DNS de la API. No toma posesión de api: el llamador
 * debe mantenerlo vivo mientras use el wa_dns_t, y liberarlo por separado
 * con wa_api_free(). */
wa_dns_t wa_dns_new(wa_api_t *api);

/*
 * Cada función devuelve:
 *   0  éxito; *out queda lleno (libéralo con su wa_dns_*_free()).
 *   1  error de la API; *out_error (si no es NULL) queda con un mensaje
 *      recién reservado — libéralo con free().
 *  -1  error de transporte; *out_error igual que arriba.
 */

/* Lista las zonas (dominios) del cliente. GET /v1/dns/zone */
int wa_dns_list_zones(const wa_dns_t *dns, wa_dns_zone_list_t *out, char **out_error);

/* Obtiene una zona (por clave numérica o por nombre de dominio) junto con
 * sus registros. GET /v1/dns/zone/{key|domain} */
int wa_dns_get_zone(const wa_dns_t *dns, const char *key_or_domain, wa_dns_zone_detail_t *out, char **out_error);

/* Crea una nueva zona. POST /v1/dns/zone */
int wa_dns_add_zone(const wa_dns_t *dns, const char *name, long *out_key, char **out_error);

/* Agrega un registro a una zona. POST /v1/dns/zone/{key}/record */
int wa_dns_add_record(const wa_dns_t *dns, long zone_key, const wa_dns_record_input_t *record,
                       long *out_key, char **out_error);

/* Modifica un registro existente. PUT /v1/dns/record/{key} */
int wa_dns_update_record(const wa_dns_t *dns, long record_key, const wa_dns_record_update_t *fields,
                          char **out_error);

/* Elimina un registro. DELETE /v1/dns/record/{key} */
int wa_dns_delete_record(const wa_dns_t *dns, long record_key, char **out_error);

/* Elimina una zona y todos sus registros. DELETE /v1/dns/zone/{key} */
int wa_dns_delete_zone(const wa_dns_t *dns, long zone_key, char **out_error);

void wa_dns_zone_free(wa_dns_zone_t *zone);
void wa_dns_record_free(wa_dns_record_t *record);
void wa_dns_zone_list_free(wa_dns_zone_list_t *list);
void wa_dns_zone_detail_free(wa_dns_zone_detail_t *detail);

#endif /* WEBABILITY_DNS_H */
