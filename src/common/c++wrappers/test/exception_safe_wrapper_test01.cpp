/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include <config.h>
#endif
}

#include <cstdlib>
#include <cerrno>
#include <string>
#include <exception>
#include <stdexcept>

#include "src/common/c++wrappers/eh_wrapper.hpp"
#include <catch2/catch_test_macros.hpp>

#define ok(EXPR, DESCR) \
    {                   \
        INFO (DESCR);   \
        CHECK ((EXPR)); \
    }

using namespace Flux::cplusplus_wrappers;
using namespace std::string_literals;

static void throw_exception (int en)
{
    switch (en) {
        case ENOMEM:
            throw std::bad_alloc ();
            break;
        case ERANGE:
            throw std::length_error (__FUNCTION__);
            break;
        case ENOENT:
            throw std::out_of_range (__FUNCTION__);
            break;
        case ENOSYS:
            throw std::exception ();
            break;
        case ENOTSUP:
            throw "Unknown exception";
            break;
    }
}

struct foo_functor_t {
    int operator() (int en)
    {
        throw_exception (en);
        return 0;
    }
};

struct foo_functor_eh_wrapper_t {
    int operator() (int en)
    {
        eh_wrapper_t exception_safe_wrapper;
        foo_functor_t functor;
        return exception_safe_wrapper (functor, en);
    }
};

int foo1 (int en)
{
    throw_exception (en);
    return 0;
}

int foo2 (int en, double p1, float p2, long p3, long long p4)
{
    int rc = (int)(p1 + p2 + p3 + p4);
    p1 = p2 = p3 = p4 = 0;
    return rc;
}

int foo3 (int en, double &p1, float &p2, long &p3, long long &p4)
{
    int rc = (int)(p1 + p2 + p3 + p4);
    p1 = p2 = p3 = p4 = 0;
    return rc;
}

void foo4 (int en)
{
    throw_exception (en);
}

TEST_CASE ("basics", "[eh_wrapper_t]")
{
    int rc = 0;
    foo_functor_eh_wrapper_t func;

    errno = 0;
    rc = func (-1);
    ok (rc == 0 && errno == 0, "eh_wrapper_t works with functor (no exception)");

    errno = 0;
    rc = func (ENOMEM);
    ok (rc == -1 && errno == ENOMEM, "eh_wrapper_t works with functor (bad_alloc exception)");

    errno = 0;
    rc = func (ERANGE);
    ok (rc == -1 && errno == ERANGE, "eh_wrapper_t works with functor (length_error exception)");

    errno = 0;
    rc = func (ENOENT);
    ok (rc == -1 && errno == ENOENT, "eh_wrapper_t works with functor (out_of_range exception)");

    errno = 0;
    rc = func (ENOSYS);
    ok (rc == -1 && errno == ENOSYS, "eh_wrapper_t works with functor (std::exception)");

    errno = 0;
    rc = func (ENOTSUP);
    ok (rc == -1 && errno == ENOSYS, "eh_wrapper_t works with functor (unknown exception)");
}

TEST_CASE ("simple function", "[eh_wrapper_t]")
{
    int rc = 0;
    const char *msg = NULL;
    eh_wrapper_t exception_safe_wrapper;

    errno = 0;
    rc = exception_safe_wrapper (foo1, -1);
    ok (rc == 0 && errno == 0, "eh_wrapper_t works with simple function (no exception)");
    ok (!exception_safe_wrapper.bad (),
        "eh_wrapper_t::bad() works with simple function (no exception)");
    msg = exception_safe_wrapper.get_err_message ();
    ok (msg == NULL, "eh_wrapper_t::get_err_message () works (no exception)");

    errno = 0;
    rc = exception_safe_wrapper (foo1, ENOENT);
    ok (rc == -1 && errno == ENOENT, "eh_wrapper_t works with simple function (out_of_range)");
    ok (exception_safe_wrapper.bad (),
        "eh_wrapper_t::bad() works with simple function (out_of_range)");
    msg = exception_safe_wrapper.get_err_message ();
    ok (msg != NULL, "eh_wrapper_t::get_err_message () reports: {}"s + msg);
}

TEST_CASE ("forward args by value", "[eh_wrapper_t]")
{
    int rc = 0;
    double p1 = 1.0f;
    float p2 = 2.0f;
    long p3 = 3;
    long long p4 = 4;
    eh_wrapper_t exception_safe_wrapper;

    errno = 0;
    int sum1 = static_cast<int> (p1 + p2 + p3 + p4);
    rc = exception_safe_wrapper (foo2, -1, p1, p2, p3, p4);
    int sum2 = static_cast<int> (p1 + p2 + p3 + p4);
    ok (rc == sum1 && sum1 == sum2 && errno == 0, "eh_wrapper_t forwards args by value works");
}

TEST_CASE ("forward args by reference", "[eh_wrapper_t]")
{
    int rc = 0;
    double p1 = 1.0f;
    float p2 = 2.0f;
    long p3 = 3;
    long long p4 = 4;
    eh_wrapper_t exception_safe_wrapper;

    errno = 0;
    int sum1 = static_cast<int> (p1 + p2 + p3 + p4);
    rc = exception_safe_wrapper (foo3, -1, p1, p2, p3, p4);
    int sum2 = static_cast<int> (p1 + p2 + p3 + p4);
    ok (rc == sum1 && sum2 == 0 && errno == 0, "eh_wrapper_t forwards args by reference works");
}

TEST_CASE ("lambda", "[eh_wrapper_t]")
{
    int rc = 0;
    const char *msg = NULL;
    eh_wrapper_t exception_safe_wrapper;

    errno = 0;
    rc = exception_safe_wrapper (
        [] (int en) {
            throw_exception (en);
            return 0;
        },
        -1);

    ok (rc == 0 && errno == 0, "eh_wrapper_t works with lambda function (no exception)");
    ok (!exception_safe_wrapper.bad (),
        "eh_wrapper_t::bad() works with simple function (no exception)");
    msg = exception_safe_wrapper.get_err_message ();
    ok (msg == NULL, "eh_wrapper_t::get_err_message () works (no exception)");

    errno = 0;
    rc = exception_safe_wrapper (
        [] (int en) {
            throw_exception (en);
            return 0;
        },
        ENOENT);
    ok (rc == -1 && errno == ENOENT, "eh_wrapper_t works with simple function (out_of_range)");
    ok (exception_safe_wrapper.bad (),
        "eh_wrapper_t::bad() works with simple function (out_of_range)");
    msg = exception_safe_wrapper.get_err_message ();
    ok (msg != NULL, "eh_wrapper_t::get_err_message () reports: {}"s + msg);
}

TEST_CASE ("void returning", "[eh_wrapper_t]")
{
    const char *msg = NULL;
    eh_wrapper_t exception_safe_wrapper;

    errno = 0;
    exception_safe_wrapper (foo4, -1);
    ok (errno == 0, "eh_wrapper_t works with void function (no exception)");
    ok (!exception_safe_wrapper.bad (),
        "eh_wrapper_t::bad() works with void function (no exception)");
    msg = exception_safe_wrapper.get_err_message ();
    ok (msg == NULL, "eh_wrapper_t::get_err_message () works void function");

    errno = 0;
    exception_safe_wrapper (foo4, ENOENT);
    ok (errno == ENOENT, "eh_wrapper_t works with void function (out_of_range)");
    ok (exception_safe_wrapper.bad (),
        "eh_wrapper_t::bad() works with void function (out_of_range)");
    msg = exception_safe_wrapper.get_err_message ();
    ok (msg != NULL, "eh_wrapper_t::get_err_message () reports: {}"s + msg);
}

/*
 * vi: ts=4 sw=4 expandtab
 */
