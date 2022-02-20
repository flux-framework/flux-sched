=====================================
flux-config-sched-fluxion-qmanager(5)
=====================================

DESCRIPTION
===========

The ``sched-fluxion-qmanager`` configuration table may be used
to tune the policies and parameters for the Fluxion graph-based
scheduler.

This table may contain the following keys:

KEYS
====

queue-policy
    (optional) String name of queueing policy to use (e.g. fcfs).

queue-params
    (optional) Comma separated list of queue parameters.

policy-params
    (optional) Comma separated list of policy paramters.


EXAMPLE
=======

::

   [sched-fluxion-qmanager]

   # queueing policy type
   queue-policy = "fcfs"

   # general queue parameters
   queue-params = "queue-depth=8192,max-queue-depth=1000000"

   # queue policy parameters
   policy-params = "reservation-depth=64,max-reservation-depth=100000"

RESOURCES
=========

Flux: http://flux-framework.org


SEE ALSO
========

:core:man5:`flux-config`



