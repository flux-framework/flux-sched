#ifndef _PRIORITY_EXTERNAL_H

#include <flux/core.h>
#include <czmq.h> // For zlist_t
#include "sched/scheduler.h" // For flux_lwj_t

namespace Flux {
namespace Priority {
extern "C" {

int sched_priority_setup (flux_t *h);
int sched_priority_record_job_usage (flux_t *h, flux_lwj_t *job);
void sched_priority_prioritize_jobs (flux_t *h, zlist_t *jobs);

} // extern "C"

int process_association_file (flux_t *h, const char *filename);

} // namespace Priority
} // namespace Flux

#endif // _PRIORITY_EXTERNAL_H
