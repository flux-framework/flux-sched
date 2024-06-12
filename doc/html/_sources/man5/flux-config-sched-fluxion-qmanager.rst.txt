=====================================
flux-config-sched-fluxion-qmanager(5)
=====================================

DESCRIPTION
===========

The ``sched-fluxion-qmanager`` configuration table may be used
to tune the queuing policies and parameters
for the Fluxion graph-based scheduler.

This table may contain the following keys:

KEYS
====

queue-policy
    (optional) String name of queuing policy to use. The
    supported policies are described in
    the :ref:`queue_policies` section. The default is "fcfs".


The following keys in the optional ``[queue-params]`` table can be
used to tune the general queuing parameters.

queue-params.max-queue-depth
    (optional) Positive integer value that sets the maximum number of pending
    jobs that can be considered per scheduling cycle.
    The default is 1000000.

queue-params.queue-depth
    (optional) Positive integer value that limits the number of pending
    jobs to consider per scheduling cycle. The default is 32.
    If it is larger than ``queue-params.max-queue-depth``, it is set to
    ``queue-params.max-queue-depth`` instead.


The following keys in the optional ``[policy-params]`` table can be
used to tune the parameters of certain queuing policies.

policy-params.max-reservation-depth
    (optional) Only applied to the ``conservative`` or ``hybrid`` policy
    that must compute the minimum start time of running
    the higher-priority pending jobs that cannot be run
    due to currently insufficient resources.
    Positive integer value that sets the maximum number of
    such higher-priority pending jobs to consider
    per scheduling cycle. The default is 100000.

policy-params.reservation-depth
    (optional) Only applied to the ``hybrid`` policy
    that must compute the minimum start time of running
    higher-priority pending jobs that cannot be run
    due to currently insufficient resources.
    Positive integer value that limits the number of
    such higher-priority pending jobs to consider
    per scheduling cycle. The default is 64.
    If it is larger than ``policy-params.max-reservation-depth``,
    it is set to ``policy-params.max-reservation-depth`` instead.


.. _queue_policies:

QUEUING POLICIES
=================

fcfs
    First come, first served policy if the priority of
    pending jobs are same: i.e., jobs are scheduled
    and run by their submission order. If pending jobs
    have different priorities, they are serviced
    by their priority order.

easy
    EASY-backfilling policy: If the highest-priority
    pending job cannot be run with ``fcfs`` because
    its requested resources are currently unavailable,
    one or more next high priority jobs will be
    scheduled and run as far as this will not delay
    the start time of running the highest-priority job.

conservative
    CONSERVATIVE-backfilling policy: Similarly to ``easy``,
    pending jobs can run out of order when the highest-priority
    job cannot run because its requested resources
    are currently unavailable. However, this policy
    is more conservative as a lower priority job can only
    be backfilled and run if and only if this will
    not delay the start time of running any pending job
    whose priority is higher than the backfilling job.

hybrid
    HYBRID-backfilling policy: This is an optimization
    of ``conservative`` where a lower priority job can only
    be backfilled and run if and only if this will
    not delay the start time of running N pending jobs
    whose priority is higher than the backfilling job.
    N can be configured by the ``policy-params.reservation-depth``
    parameter: see ``policy-params.reservation-depth``


EXAMPLE
=======

::

   [sched-fluxion-qmanager]

   # queuing policy type
   queue-policy = "hybrid"

   # general queue parameters
   [sched-fluxion-qmanager.queue-params]

   max-queue-depth = 1000000
   queue-depth = 8192

   # queue policy parameters
   [sched-fluxion-qmanager.policy-params]

   max-reservation-depth = 100000
   reservation-depth = 64


RESOURCES
=========

Flux: http://flux-framework.org


SEE ALSO
========

:core:man5:`flux-config`



