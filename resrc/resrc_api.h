#ifndef _FLUX_RESRC_API_H
#define _FLUX_RESRC_API_H

#define REDUCE_UNDER_ME 1
#define GATHER_UNDER_ME 2
#define NONE_UNDER_ME 3

typedef struct resrc_api_ctx resrc_api_ctx_t;
typedef struct resrc_api_map resrc_api_map_t;

/*
 * Create and initialize an instance of resource API and
 * and return its API context. Using the context, each
 * API instance can be specialized and multiple instances
 * used at the same time.
 */
resrc_api_ctx_t *resrc_api_init (void);

/*
 * Destroy the API context passed in from the caller. On encountering
 * an error, set errno:
 *     EINVAL: invalid ctx passed in
 */
void resrc_api_fini (resrc_api_ctx_t *ctx);

resrc_api_map_t *resrc_api_map_new ();
void resrc_api_map_destroy (resrc_api_map_t **m);
void *resrc_api_map_get (resrc_api_map_t *m, const char *key);
void resrc_api_map_put (resrc_api_map_t *m, const char *key, void *val);
void resrc_api_map_rm (resrc_api_map_t *m, const char *key);

#endif /* !_FLUX_RESRC_API_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
