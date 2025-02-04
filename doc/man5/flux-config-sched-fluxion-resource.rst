=====================================
flux-config-sched-fluxion-resource(5)
=====================================

DESCRIPTION
===========

The ``sched-fluxion-resource`` configuration table may be used
to tune the resource match policies and parameters
for the Fluxion graph-based scheduler.

This table may contain the following keys:

KEYS
====

match-policy
    (optional) String name of match policy to use. The supported
    match polices are described in the :ref:`match_policies` section.
    The default is "first".

match-format
    (optional) String name of match format to use.
    "rv1" and "rv1_nosched" are currently supported.
    When a job is allocated, its resource set is encoded
    in RFC 20 Resource Set Specification Version 1.
    It has an optional ``scheduling`` key and this is
    only encoded by "rv1".
    By omitting the ``scheduling`` key, "rv1_nosched" will
    result in higher scheduling performance. However,
    this format will not contain sufficient
    information to reconstruct the state
    of ``sched-fluxion-resource`` on module reload (as
    required for system instance failure recovery).
    The default is "rv1_nosched".

load-allowlist
    (optional) Comma-separated list of resource types to load
    with the ``hwloc`` reader.
    When Flux is instantiated in single-user mode
    with a foreign workload manager (e.g., IBM LSF, SLURM, etc),
    ``sched-fluxion-resource`` can discover the target resources
    by using ``hwloc``. This list allows ``sched-fluxion-resource``
    to load only the resources of the specified types
    from our ``hwloc`` reader as needed for scheduling.

reserve-vtx-vec
    (optional) Integer value that reserves memory to store
    the specified number of graph vertices in order
    to optimize resource-graph loading performance.
    Recommended for handling large-scale systems.
    The value must be a non-zero integer up to 2000000.

prune-filters
    (optional) Comma-separated list of graph-search filters
    to accelerate match operations. Each filter is
    expressed such that a High-Level (HL) resource
    vertex can track the aggregate state of the Low-Level (LL) resources
    residing under its subtree graph.
    If a jobspec requests 1 compute node with 4 cores, and the visiting
    compute-node vertex has only a total of 2 available cores
    in aggregate at its subtree, this filter allows the traverser
    to prune a further descent to accelerate the search.
    The format must conform to
    ``<HL-resource1:LL-resource1[,HL-resource2:LL-resource2...]...]>``.
    Use the ``ALL`` keyword for HL-resource if you want LL-resource
    to be tracked at all of its ancestor HL-resource vertices.
    The default is "ALL:core".


.. _match_policies:

RESOURCE MATCH POLICIES
=======================

low
    Select resources with low ID first (e.g., core0 is selected
    first before core1 is selected).

high
    Select resources with high ID first (e.g., core15 is selected
    first before core14).

lonode
    Select resources with lowest compute-node ID first; otherwise
    the ``low`` policy (e.g., for node-local resource types).

hinode
    Select resources with highest compute-node ID first; otherwise
    the ``high`` policy (e.g., for node-local resource types).

lonodex
    A node-exclusive scheduling whose behavior is
    identical to ``lonode`` except each compute node
    is exclusively allocated.

hinodex
    A node-exclusive scheduling whose behavior is
    identical to ``hinode`` except each compute node
    is exclusively allocated.

first
    Select the first matching resources and stop the search

firstnodex
    A node-exclusive scheduling whose behavior is identical to
    ``first`` except each compute node is exclusively allocated.


CUSTOM MATCH POLICIES
=====================
Match policies are initialized as collections of individual attributes
that help the scheduler to select matches. For convenience, these 
attributes are exposed to users such that they can write custom policies.
Below is a list of match attributes which can be selected by users.

policy
    Allowed options are ``low`` or ``high``. If only ``policy=low``
    or ``policy=high`` is specified, the behavior of the match policy is the
    same as if ``match-policy=low`` or ``match-policy=high`` were selected,
    respectively.

node_centric
    ``true`` or ``false`` are allowed options. Evaluate matches based on the
    ID of the compute node first. 

node_exclusive
    ``true`` or ``false`` are allowed options. Exclusively allocate compute
    nodes when a match is found.

set_stop_on_1_matches
    ``true`` or ``false`` are allowed options. When a match is found, take
    it, without evaluating for potentially more optimal matches.

::

    [sched-fluxion-resource]

    # system instance will use node-exclusive
    # scheduling first-match (with nodes of high node IDs
    # selected first).
    match-policy = "policy=high node_exclusive=true set_stop_on_1_matches=true"


EXAMPLE
=======

::

    [sched-fluxion-resource]

    # system instance will use node-exclusive
    # scheduling (with nodes of low node IDs
    # selected first).
    match-policy = "lonodex"

    # system-instance will use full-up rv1 writer
    # so that R will contain scheduling key needed
    # for failure recovery.
    match-format = "rv1"


RESOURCES
=========

Flux: http://flux-framework.org

RFC 20: Resource Set Specification Version 1: https://flux-framework.rtfd.io/projects/flux-rfc/en/latest/spec_20.html

SEE ALSO
========

:core:man5:`flux-config`

