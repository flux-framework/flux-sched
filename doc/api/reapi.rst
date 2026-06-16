.. _api-resource:

.. attention::
   Fluxion/flux-sched (both refer to the same project) is under development
   and its interfaces are not yet stable. APIs are not exported or exposed
   to external consumers. This documentation is meant for developers of
   Fluxion.

REAPI
*****

**C Interface**
===============

.. doxygenfile:: resource/reapi/bindings/c/reapi_cli.h
   :project: flux-sched

.. doxygenfile:: resource/reapi/bindings/c/reapi_module.h
   :project: flux-sched

**C++ Interface**
=================

.. doxygenfile:: resource/reapi/bindings/c++/reapi.hpp
   :project: flux-sched

.. doxygenfile:: resource/reapi/bindings/c++/reapi_cli.hpp
   :project: flux-sched

.. doxygenfile:: resource/reapi/bindings/c++/reapi_module.hpp
   :project: flux-sched
