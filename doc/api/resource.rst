.. _api-resource:

.. attention::
   Fluxion/flux-sched (both refer to the same project) is under development
   and its interfaces are not yet stable. APIs are not exported or exposed
   to external consumers. This documentation is meant for developers of
   Fluxion.


Resource
********

Resource graph and matching infrastructure.

**Evaluators**

.. doxygenfile:: resource/evaluators/expr_eval_api.hpp
   :project: flux-sched

.. _api-traversers:

**Traversers**

Graph traversal implementations for resource matching.

.. doxygenfile:: resource/traversers/dfu.hpp
   :project: flux-sched

.. _api-policies:

**Policies**

Resource selection policies for graph traversal and matching.

.. doxygenfile:: resource/policies/base/dfu_match_cb.hpp
   :project: flux-sched
