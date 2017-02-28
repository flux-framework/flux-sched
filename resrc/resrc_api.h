#ifndef _FLUX_RESRC_API_H
#define _FLUX_RESRC_API_H


typedef struct resrc_api_ctx resrc_api_ctx_t;

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

/* Pls add other API-level specialization functions here */

#endif /* !_FLUX_RESRC_API_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
