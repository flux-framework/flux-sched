#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <argz.h>
#include <czmq.h>

#include <cassert>
#include <memory>
#include <string>
#include <vector>

#include <flux/core.h>

#include "src/common/libutil/shortjansson.h"
#include "sched/scheduler.h"
#include "priority_external_api.hpp"
#include "modified_fair_tree.hpp"

using namespace std;

namespace Flux {
namespace Priority {

typedef struct {
    const char *name;
    double weight;
    double (*fn)(flux_lwj_t *job);
} job_priority_factor_t;

vector<job_priority_factor_t> prio_factors; /* Job priority factors */
priority_tree ptree;

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
 *
 * Create the associations file that this plugin requires by invoking:
 *   sacctmgr -n -p show assoc cluster=<cluster name> format=account,share,parentn,user > associations
 *
 * Return the number of successfully read associations.
 */
int process_association_file (flux_t *h, const char *filename)
{
    FILE        *fp;
    char        line[1028];
    char        *ptr;

    string account;
    unsigned long shares = 1.0;
    string parent_name;
    string user;

    int n_assoc = 0;

    fp = fopen (filename, "r");
    if (fp == NULL) {
        flux_log (h, LOG_ERR, "%s: failed to open %s", __FUNCTION__, filename);
        return -1;
    }

    ptree.update_start();
    unordered_set<string> seen_accounts;
    while (fgets (line, sizeof(line), fp) != NULL) {
        char *field = line;

        /* Account is a required field */
        ptr = strchr (field, '|');
        if (ptr == NULL) {
            flux_log (h, LOG_ERR,
                      "%s: Could not find account in line \"%s\"",
                      __FUNCTION__, field);
            continue;
        } else if (ptr == field) {
            flux_log (h, LOG_ERR,
                      "%s: Account field is empty in line \"%s\"",
                      __FUNCTION__, field);
            continue;
        } else {
            *ptr = '\0';
            account = string(field);
        }
        field = ptr + 1;

        /* Share is a required field */
        ptr = strchr (field, '|');
        if (ptr == NULL) {
            flux_log (h, LOG_ERR,
                      "%s: Could not find shares in line subset \"%s\"",
                      __FUNCTION__, field);
            continue;
        } else if (ptr == field) {
            flux_log (h, LOG_ERR,
                      "%s: Share field is empty in line \"%s\"",
                      __FUNCTION__, field);
            continue;
        } else {
            *ptr = '\0';
            errno = 0;
            long val = strtol (field, NULL, 10);
            if (val == 0 && errno != 0) {
                flux_log (h, LOG_ERR,
                          "%s: Share field is not a number \"%s\"",
                          __FUNCTION__, field);
                continue;
            } else if ((val == LONG_MIN || val == LONG_MAX) && errno != 0) {
                flux_log (h, LOG_ERR,
                          "%s: Share field is out of long range \"%s\"",
                          __FUNCTION__, field);
                continue;
            } else if (val < 0) {
                flux_log (h, LOG_ERR,
                          "%s: Share field is negative \"%s\"",
                          __FUNCTION__, field);
                continue;
            }
            shares = (unsigned long) val;
        }
        field = ptr + 1;

        /* Parent Name is an optional field */
        ptr = strchr (field, '|');
        if (ptr == NULL) {
            flux_log (h, LOG_ERR,
                      "%s: Could not find parent in line subset \"%s\"",
                      __FUNCTION__, field);
            continue;
        } else if (ptr != field) {
            *ptr = '\0';
            parent_name = string(field);
        } else {
            parent_name.clear();
        }
        field = ptr + 1;

        /* User is a an optional field */
        ptr = strchr (field, '|');
        if (ptr == NULL) {
            flux_log (h, LOG_ERR,
                      "%s: Could not find user in line subset \"%s\"",
                      __FUNCTION__, field);
            continue;
        } else if (ptr != field) {
            *ptr = '\0';
            user = string(field);
        } else {
            user.clear();
        }

        /*
         * In the ordering accounts in the sacctmgr output, parent
         * accounts appear before child accounts.  Hence
         * parent_assoc should always found and be non-NULL.  The
         * only exception is the root account - it has no parent.
         * FIXME: this assumption may not always be valid
         */
        if (account == string("root") && user.empty()) {
            // This is the root account.  Anything listed before this
            // will be discarded later in this function.  A priority_tree
            // always has a root node that it creates on its own; we
            // don't need to add it.
            if (!parent_name.empty())
                flux_log (h, LOG_WARNING,
                          "The root account cannot have a parent");
            seen_accounts.insert (string ("root"));
        } else if (user.empty()) {
            /* this is an account */
            try {
                ptree.update_add_account (account, parent_name, shares);
            } catch (std::exception &e) {
                flux_log (h, LOG_WARNING, "Ignoring parentless: "
                          "Account=\"%s\" Share=\"%lu\" Par Name=\"%s\" User=\"%s\"",
                          account.c_str(), shares, parent_name.c_str(), user.c_str());
                continue;
            }
            pair<unordered_set<string>::iterator, bool> result;
            result = seen_accounts.insert (account);
            if (result.second == false) {
                // account already appears in seen_accounts
                flux_log (h, LOG_WARNING,
                          "Account \"%s\" appears more than once in input",
                          account.c_str());
                continue;
            }
        } else {
            /* this is a user */
            if (seen_accounts.count (account) != 1) {
                flux_log (h, LOG_WARNING, "Parent account not seen in input: "
                          "Account=\"%s\" Share=\"%lu\" Par Name=\"%s\" User=\"%s\"",
                          account.c_str(), shares, parent_name.c_str(), user.c_str());
                continue;
            }
            try {
                ptree.update_add_user (user, account, shares);
            } catch (std::exception &e) {
                flux_log (h, LOG_WARNING, "Parent account not in tree: "
                          "Account=\"%s\" Share=\"%lu\" Par Name=\"%s\" User=\"%s\"",
                          account.c_str(), shares, parent_name.c_str(), user.c_str());
                continue;
            }
        }

        flux_log (h, LOG_DEBUG, "Processed: "
                  "Account=\"%s\" Share=\"%lu\" Par Name=\"%s\" User=\"%s\"",
                  account.c_str(), shares, parent_name.c_str(), user.c_str());
        n_assoc++;
    }
    fclose(fp);
    ptree.update_finish();
    flux_log (h, LOG_INFO, "priority plugin loaded %d associations", n_assoc);

    return n_assoc;
}

/* Return the number of seconds that have elapsed since the job was
 * submitted.  The longer the job has been waiting in the queue, the
 * greater its wait_time priority component.
 */
double wait_time (flux_lwj_t *job)
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
double fair_share (flux_lwj_t *job)
{
    assert (job->account != NULL);
    assert (job->user != NULL);

    return ptree.get_fair_share_factor(job->account, job->user);
}

double qos (flux_lwj_t *job)
{
    /*
     * TODO: Return the value associated with a Quality of Service
     */
    return 0.0;
}

double queue (flux_lwj_t *job)
{
    /*
     * TODO: Return the value associated with Flux's equivalent of a
     * node partition (job queue)
     */
    return 0.0;
}

double job_size (flux_lwj_t *job)
{
    /*
     * TODO: If smaller jobs should receive the higher priority, then
     * the total number of cores in the cluster will need to be passed
     * in.  The return value would then change to:
     *   ncores_in_cluster - job->req->nnodes * job->req->ncores
     */
    return job->req->nnodes * job->req->ncores;
}

double user (flux_lwj_t *job)
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

extern "C"
int sched_priority_setup (flux_t *h)
{
    int num_assoc;
    job_priority_factor_t* jpf = NULL;

    jpf = &jpfs[0];
    while (jpf->name) {
        prio_factors.push_back(*jpf);
        jpf++;
    }

    num_assoc = process_association_file (h, "associations");
    if (num_assoc == 0)
        return -1;

    return 0;
}

/*
 * Usage is currently implemented as core*seconds.  This could
 * eventually expand to include memory, GPUs, power, etc.  The scalar
 * "usage" value is intended to represent a weighted composite of all
 * of the charge-able, compute-related resources.  record_job_usage()
 * will meter that data at the completion of every job.
 */
extern "C"
int sched_priority_record_job_usage (flux_t *h, flux_lwj_t *job)
{
    assert (job->account != NULL);
    assert (job->user != NULL);

    double usage = (job->endtime - job->starttime)
        * job->req->nnodes * job->req->ncores;
    ptree.add_usage (job->account, job->user, usage);

    return 0;
}

extern "C"
void sched_priority_prioritize_jobs (flux_t *h, zlist_t *jobs)
{
    ptree.calc_fs_factors ();

    flux_lwj_t *job = (flux_lwj_t *) zlist_first (jobs);
    while (job) {
        job->priority = 0;
        for (auto && factor: prio_factors) {
            job->priority += factor.weight * factor.fn(job);
        }
        job = (flux_lwj_t *) zlist_next (jobs);
    }
}

} // namespace Priority
} // namespace Flux

MOD_NAME("sched.priority_fair_tree");
/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

