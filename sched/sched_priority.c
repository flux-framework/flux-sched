#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argz.h>
#include <libgen.h>
#include <errno.h>
#include <libgen.h>
#include <czmq.h>

#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/shortjansson.h"
#include "src/common/libutil/xzmalloc.h"
#include "scheduler.h"

typedef struct {
    char   *name;
    double weight;
    double (*fn)(flux_lwj_t *job);
} job_priority_factor_t;

static zlist_t *prio_factors;          /* Job priority factors */
static zhash_t *associations;          /* charge account / user associations */

/*
 * An association contains the shares of computing resources assigned
 * to a charge account and optionally the user permitted to charge
 * that account.  There can be a hierarchy of associations with the
 * "root" account at the top.  Each level in the hierarchy contains
 * associations of associations with the leaves being associations of
 * users.
 *
 * For example, if a user is permitted to charge the "physics"
 * account, there will be an association of that user and account
 * "physics".  A different association will exist for every other user
 * permitted to charge the physics account.
 *
 * If the physics account is a child of the "science" account, then
 * both the physics and science associations will have their user
 * member set to NULL.
 *
 * In addition to shares, an association records the usage of
 * computing resource, which is nominally in units of core * seconds.
 *
 * The fair-share factor for the association represents the difference
 * between the assigned shares and accrued usage.  The fair-share
 * factor ranges from 0.0 to 1.0 where:
 *   0.0 represents an over-serviced association
 *   0.5 usage is commensurate with assigned shares
 *   1.0 association has no accrued usage
 */
typedef struct association association_t;
struct association {
    char   *account;
    char   *user;
    double shares;
    double sibling_shares;      /* sum of all children's shares */
    double usage;               /* historical cpu-second sum */
    double sibling_usage;       /* sum of all children's usage */
    double low_factor;          /* fair-share factor of next lowest sibling */
    double fs_factor;           /* normalized fair-share factor */
    association_t *parent;
    zlist_t *children;
};


static association_t *create_association (char* account, char *user, double shares,
                                          association_t *parent)
{
    association_t *assoc = xzmalloc (sizeof (association_t));

    assoc->account = account;
    assoc->user = user;
    assoc->shares = shares;
    assoc->usage = 0.0;
    assoc->low_factor = 0.0;
    assoc->fs_factor = 1.0;
    assoc->parent = parent;

    return assoc;
}

/*
 * Create the associations file that this plugin requires by invoking:
 * sacctmgr -n -p show assoc cluster=$LCSCHEDCLUSTER
 *   format=account,share,parentn,user > associations
 */
static int read_association_file (flux_t *h)
{
    FILE        *fp;
    association_t *assoc;
    association_t *parent_assoc;
    char        line[1028];
    char        *account = NULL;
    char        *field = NULL;
    char        *filename = "associations";
    char        *key;
    char        *parent_name = NULL;
    char        *ptr;
    char        *user = NULL;
    double      shares = 1.0;
    int         n_assoc = 0;
    int         rc = 0;

    if ((fp = fopen (filename, "r"))) {
        while (fgets (line, sizeof(line), fp) != NULL) {
            if ((ptr = strchr (line, '|'))) {
                *ptr = '\0';
                account = line;
                field = ptr + 1;
            }
            if ((ptr = strchr (field, '|'))) {
                *ptr = '\0';
                shares = atof (field);
                field = ptr + 1;
            }
            if ((ptr = strchr (field, '|'))) {
                *ptr = '\0';
                parent_name = field;
                field = ptr + 1;
            }
            if ((ptr = strchr (field, '|'))) {
                *ptr = '\0';
                user = field;
            }
            /*
             * In the ordering accounts in the sacctmgr output, parent
             * accounts appear before child accounts.  Hence
             * parent_assoc should always found and be non-NULL.  The
             * only exception is the root account - it has no parent.
             */
            parent_assoc = zhash_lookup (associations, parent_name);

            if ((assoc = create_association (account, user, shares, parent_assoc))) {
                if (user)
                    /* the key will be a fusing of the account and user name */
                    key = xasprintf ("%s-%s", account, user);
                else
                    /* this is an account, so the key is just the account */
                    key = xasprintf ("%s", account);
                zhash_insert (associations, key, assoc);
                zhash_freefn (associations, key, free);
                n_assoc++;
            } else {
                flux_log (h, LOG_ERR, "%s: failed to create association %s-%s", __FUNCTION__,
                          account, user);
                rc = -1;
            }
        }
        fclose(fp);
        flux_log (h, LOG_INFO, "priority plugin loaded %d associations", n_assoc);
    } else {
        flux_log (h, LOG_ERR, "%s: failed to open %s", __FUNCTION__, filename);
        rc = -1;
    }

    return rc;
}

/* Computes the sum of raw shares and used resources across all
 * siblings */
static void sum_sibling_shares_usage (association_t *a)
{
    association_t *sib;
    zlist_t *sibs = a->children;

    a->sibling_shares = 0.0;
    a->sibling_usage = 0.0;
    sib = zlist_first (sibs);
    while (sib) {
        a->sibling_shares += sib->shares;
        a->sibling_usage += sib->usage;
        sib = zlist_next (sibs);
    }
}

/*
 * The formula for the fair-share factor is:
 *   fs = 2**(-U/S)
 * where:
 * U = my_usage / clusters_usage
 * S = ((my_shares / all_siblings_shares) *
 *      (my_parents_shares / all_parents_siblings_shares) *
 *      (grandparents_shares / all_grandparents_siblings_shares) *
 *      ...
 * The fair-share factor ranges from 0.0 to 1.0:
 *  0.0 - represents an over-serviced association
 *  0.5 - usage is commensurate with assigned shares
 *  1.0 - association has no accrued usage
 *
 * Plotted, it looks like this:
 * http://www.wolframalpha.com/input/?i=Plot%5B2**(-u%2Fs)%5D,+%7Bu,+0.,+1.%7D,+%7Bs,+0.,+1.%7D
 *
 * While this gives us our fs factor relative to our siblings, we need
 * to "inherit" the fair-share of our parent's account.  Our parent's
 * fair-share factor.  Pictorially, it looks like this:
 *
 * 0.0                                                   1.0
 *  |-----------------------------------------------------|
 *  |----------|--------------------|-------------------|-|
 *             A                    B                   C
 *             |----|--------|------|
 *                  U1       U2     U3
 *
 * where A, B, and C are three accounts plotted at their fair-share
 * value.  U3 is a user in account B who has no accrued usage.  Hence,
 * U3's fair-share on its own is 1.0.  However, it inherits its parent
 * (B)'s fair-share value.  U1 and U2 have their own fair-share values
 * of 0.3 and 0.7.  However, these also inherit parent B's fairshare.
 * This is done by multiplying (one minus their local values) by the
 * the difference between their parent (B)'s fair-share value and the
 * fair-share value of the next lower sibling of their parent: A in
 * this case; and then subtracting this product from their parent B's
 * fair-share value.
 *
 * You'll see this formula in calc_fs_factor()
 */

static void calc_fs_factor (association_t *a)
{
    association_t *p = a->parent;
    double my_factor;
    double norm_shares;
    double norm_usage;
    double parent_factor_range;

    norm_shares = a->shares / a->sibling_shares;
    norm_usage = a->usage / a->sibling_usage;
    if (norm_shares > 0.0)
        my_factor = pow (2.0, -(norm_usage/norm_shares));
    else
        my_factor = 0.0;
    parent_factor_range = p->fs_factor - p->low_factor;

    a->fs_factor = p->fs_factor - (1.0 - my_factor) * parent_factor_range;
}

/*
 * Sort the associations by their fair-share factor: low to high
 */
static int compare_factors (void *item1, void *item2)
{
    int ret = 0;
    association_t *sib1 = (association_t*)item1;
    association_t *sib2 = (association_t*)item2;

    if (sib1->fs_factor < sib2->fs_factor)
        ret = 1;
    else if (sib1->fs_factor > sib2->fs_factor)
        ret = -1;
    return ret;
}

static void calc_sibling_fs_factors (association_t *a)
{
    association_t *p = a->parent;
    association_t *sib;
    double last_low_factor;
    double sav_low_factor;

    sum_sibling_shares_usage (a);

    sib = zlist_first (a->children);
    while (sib) {
        calc_fs_factor (sib);
        sib = zlist_next (a->children);
    }

    /* Sort sibling fair-share factors - low to high */
    zlist_sort (a->children, compare_factors);

    last_low_factor = p->low_factor;
    sav_low_factor = last_low_factor;
    sib = zlist_first (a->children);
    while (sib) {
        /*
         * We need to support successive siblings with the same
         * fs_factor.  All siblings with the same fs_factor should be
         * assigned the same low_factor
         */
        if (sib->fs_factor == last_low_factor)
            sib->low_factor = sav_low_factor;
        else {
            sib->low_factor = last_low_factor;
            sav_low_factor = last_low_factor;
            last_low_factor = sib->fs_factor;
        }
        sib = zlist_next (a->children);
    }
}

/*
 * A recursive function that is first called for the root association.
 * All siblings' fair-share factors are computed before children are
 * considered.
 */
static void calc_fs_factors (association_t *a)
{
    association_t *child;

    calc_sibling_fs_factors (a);
    child = zlist_first (a->children);
    while (child) {
        calc_fs_factors (child);
        child = zlist_next (a->children);
    }
}

/* Return the number of seconds that have elapsed since the job was
 * submitted.  The longer the job has been waiting in the queue, the
 * greater its wait_time priority component.
 */
static double wait_time (flux_lwj_t *job)
{
    if (job->submittime)
        return ((double)time (NULL) - job->submittime);
    return 0.0;
}

/*
 * Retrieve the latest fair-share factor associated with this user and
 * account.  If that association can't be found, return a zero
 * fair-share factor.
 */
static double fair_share (flux_lwj_t *job)
{
    association_t *assoc;
    char *key;
    double fs_factor = 0.0;

    key = xasprintf ("%s-%s", job->account, job->user);
    assoc = zhash_lookup (associations, key);
    if (assoc)
        fs_factor = assoc->fs_factor;
    free (key);

    return fs_factor;
}

static double qos (flux_lwj_t *job)
{
    /*
     * TODO: Return the value associated with a Quality of Service
     */
    return 0.0;
}

static double queue (flux_lwj_t *job)
{
    /*
     * TODO: Return the value associated with Flux's equivalent of a
     * node partition (job queue)
     */
    return 0.0;
}

static double job_size (flux_lwj_t *job)
{
    /*
     * TODO: If smaller jobs should receive the higher priority, then
     * the total number of cores in the cluster will need to be passed
     * in.  The return value would then change to:
     *   ncores_in_cluster - job->req->nnodes * job->req->ncores
     */
    return job->req->nnodes * job->req->ncores;
}

static double user (flux_lwj_t *job)
{
    /* User allowed to decrease their job's priority only */
    if (job->user_prio < 0.0)
        return (job->user_prio);
    return 0.0;
}

job_priority_factor_t jpfs[] = {
    { "WaitTime", 100.0, wait_time},
    { "FairShare", 200.0, fair_share},
    { "QoS", 300.0, qos},
    { "Queue", 400.0, queue},
    { "JobSize", 500.0, job_size},
    { "User", 600.0, user},
    { NULL, 0.0, NULL}
};

int priority_setup (flux_t *h)
{
    int rc = -1;
    job_priority_factor_t* jpf = NULL;

    if (!(prio_factors = zlist_new ()))
        oom ();

    if (!(associations = zhash_new ()))
        oom ();

    jpf = &jpfs[0];
    while (jpf->name) {
        zlist_append (prio_factors, jpf);
        jpf++;
    }

    rc = read_association_file (h);
    return rc;
}

/*
 * Record usage for this association and all parent associations up to
 * and including the root association.
 */
void record_association_usage (association_t *assoc, double usage)
{
    if (assoc) {
        assoc->usage += usage;
        if (assoc->parent)
            record_association_usage (assoc->parent, usage);
    }
}

/*
 * Usage is currently implemented as core*seconds.  This could
 * eventually expand to include memory, GPUs, power, etc.  The scalar
 * "usage" value is intended to represent a weighted composite of all
 * of the charge-able, compute-related resources.  record_job_usage()
 * will meter that data at the completion of every job.
 */
int record_job_usage (flux_t *h, flux_lwj_t *job)
{
    association_t *assoc;
    char *key;
    double usage;
    int rc = -1;

    key = xasprintf ("%s-%s", job->account, job->user);
    assoc = zhash_lookup (associations, key);
    if (assoc) {
        usage = (job->endtime - job->starttime) * job->req->nnodes * job->req->ncores;
        record_association_usage (assoc, usage);
        rc = 0;
    }
    free (key);

    return rc;
}

void prioritize_jobs (flux_t *h, zlist_t *jobs)
{
    flux_lwj_t *job = NULL;
    job_priority_factor_t *factor = NULL;
    association_t *root = zhash_lookup (associations, "root");

    if (root) {
        calc_fs_factors (root);

        job = zlist_first (jobs);
        while (job) {
            job->priority = 0;
            factor = zlist_first (prio_factors);
            while (factor) {
                job->priority += factor->weight * factor->fn(job);
                factor = zlist_next (prio_factors);
            }
            job = zlist_next (jobs);
        }
    }
}

MOD_NAME ("sched.priority");


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

