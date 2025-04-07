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

#include <vector>
#include <iostream>
#include <algorithm>
#include "resource/evaluators/expr_eval_api.hpp"
#include "resource/evaluators/expr_eval_target.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux;
using namespace Flux::resource_model;

class expr_eval_test_target_t : public expr_eval_target_base_t {
   public:
    virtual int validate (const std::string &p, const std::string &x) const override;

    virtual int evaluate (const std::string &p, const std::string &x, bool &result) const override;

    virtual int extract (
        const std::string &p,
        const std::string &x,
        std::vector<std::pair<std::string, std::string>> &predicates) const override;
};

int expr_eval_test_target_t::validate (const std::string &p, const std::string &x) const
{
    if (p != "status" && p != "sched-now" && p != "jobid-alloc" && p != "agfilter")
        return -1;
    if (p == "status" && (x != "up" && x != "down"))
        return -1;
    if (p == "sched-now" && (x != "allocated" && x != "free"))
        return -1;
    if (p == "jobid-alloc") {
        try {
            std::stoul (x);
        } catch (std::out_of_range) {
            return -1;
        }
    }
    if (p == "agfilter" && (x != "true")) {
        return -1;
    }
    return 0;
}

int expr_eval_test_target_t::evaluate (const std::string &p,
                                       const std::string &x,
                                       bool &result) const
{
    int rc = -1;
    if ((rc = validate (p, x)) < 0)
        return rc;
    result = true;
    return rc;
}

int expr_eval_test_target_t::extract (
    const std::string &p,
    const std::string &x,
    std::vector<std::pair<std::string, std::string>> &predicates) const
{
    std::string lcx = x;
    if (validate (p, x) < 0)
        return -1;
    std::transform (x.begin (), x.end (), lcx.begin (), ::tolower);
    predicates.push_back ({p, lcx});
    return 0;
}

void build_simple_expr (std::vector<std::string> &expr_vector)
{
    expr_vector.push_back ("  status=up      ");
    expr_vector.push_back ("  status=up          sched-now=allocated    	");
    expr_vector.push_back ("status=up and sched-now=allocated");
    expr_vector.push_back ("status=up or sched-now=allocated");
    expr_vector.push_back ("status=up sched-now=free");
    expr_vector.push_back ("status=up   and sched-now=free");
    expr_vector.push_back ("status=up or sched-now=free");
    expr_vector.push_back ("status=down   sched-now=allocated");
    expr_vector.push_back ("status=down and sched-now=allocated");
    expr_vector.push_back ("status=down or sched-now=allocated");
    expr_vector.push_back ("status=down sched-now=free");
    expr_vector.push_back ("status=down and sched-now=free");
    expr_vector.push_back ("status=down or sched-now=free");
    expr_vector.push_back ("sched-now=allocated status=up");
    expr_vector.push_back ("sched-now=allocated and status=up");
    expr_vector.push_back ("sched-now=allocated or status=up");
    expr_vector.push_back ("sched-now=free or status=up");
    expr_vector.push_back ("sched-now=free or   status=up");
    expr_vector.push_back ("sched-now=free or status=up");
    expr_vector.push_back ("sched-now=allocated or status=down");
    expr_vector.push_back ("sched-now=allocated and status=down");
    expr_vector.push_back ("sched-now=allocated or status=down");
    expr_vector.push_back ("sched-now=free status=down");
    expr_vector.push_back ("sched-now=free and status=down");
    expr_vector.push_back ("sched-now=free  or          status=down");
    expr_vector.push_back ("agfilter=true");
    expr_vector.push_back ("jobid-alloc=1");
    expr_vector.push_back ("jobid-alloc=1 and agfilter=true");
}

void build_paren_expr (std::vector<std::string> &expr_vector)
{
    expr_vector.push_back (
        "(status=up and sched-now=allocated) "
        "and (sched-now=free and status=down)");
    expr_vector.push_back (
        "status=up and sched-now=allocated "
        "and (sched-now=free and status=down)");
    expr_vector.push_back (
        "status=up and sched-now=allocated "
        "and sched-now=free and status=down  ");
    expr_vector.push_back ("status=up sched-now=allocated (( sched-now=free status=down))");
    expr_vector.push_back (
        "status=up sched-now=allocated "
        "(( sched-now=free and (status=down or sched-now=allocated)))");
    expr_vector.push_back (
        "status=up sched-now=allocated "
        "(sched-now=free (status=down or sched-now=allocated))");
    expr_vector.push_back (
        "sched-now=free and "
        "(status=down or sched-now=allocated) and status=up");
}

void build_invalid_expr (std::vector<std::string> &expr_vector)
{
    expr_vector.push_back ("status=up sched-now=allocated (( sched=free status=down)");
    expr_vector.push_back ("status=up and sched-now=foo");
    expr_vector.push_back ("sched-now=free status=down and");
    expr_vector.push_back ("sched-now=free status=d own");
    expr_vector.push_back (
        "(status=up (sched-now=allocated) "
        "and (sched-now=free and) status=down)");
    expr_vector.push_back (
        "(status=up (sched-now=allocated) "
        "and (sched-now=free)status=down)");
    expr_vector.push_back (
        "(status=up)(sched-now=allocated)"
        "(sched-now=free)(status=down)");
    expr_vector.push_back ("(status=up and sched-now=allocated))");
    expr_vector.push_back ("(status=up and sched-now=allocated)))");
    expr_vector.push_back ("jobid-alloc=18446744073709551616");
}

void build_extract_expr (std::vector<std::string> &expr_vector)
{
    expr_vector.push_back ("jobid-alloc=1");
    expr_vector.push_back ("jobid-alloc=10");
    expr_vector.push_back ("jobid-alloc=10 and agfilter=true");
    expr_vector.push_back ("agfilter=true");
}

void test_validation (std::vector<std::string> &expr_vector,
                      const std::string &label,
                      bool must_success)
{
    int rc = 0;
    bool expected = false;
    Flux::resource_model::expr_eval_api_t evaluator;
    expr_eval_test_target_t expr_eval_test_target;

    for (const auto &expr : expr_vector) {
        rc = evaluator.validate (expr, expr_eval_test_target);
        expected = must_success ? rc == 0 : rc < 0;
        ok (expected, "%s: ^%s$", label.c_str (), expr.c_str ());
    }
}

void test_evaluation (std::vector<std::string> &expr_vector,
                      const std::string &label,
                      bool must_success)
{
    int rc = 0;
    bool expected = false;
    bool expected_result = false;
    bool result = false;
    Flux::resource_model::expr_eval_api_t evaluator;
    expr_eval_test_target_t expr_eval_test_target;

    for (const auto &expr : expr_vector) {
        result = false;
        rc = evaluator.evaluate (expr, expr_eval_test_target, result);
        expected = must_success ? rc == 0 : rc < 0;
        expected_result = must_success ? result : !result;
        ok (expected && expected_result, "%s: ^%s$", label.c_str (), expr.c_str ());
    }
}

void test_extraction (std::vector<std::string> &expr_vector,
                      std::vector<unsigned long> &jobids,
                      std::vector<bool> &agfilters,
                      const std::string &label,
                      bool must_success)
{
    int rc = 0;
    int i = 0;
    bool expected = false;
    bool expected_jobid = false;
    bool expected_agfilter = false;
    bool agfilter = false;
    unsigned long jobid = 0;
    Flux::resource_model::expr_eval_api_t evaluator;
    expr_eval_test_target_t expr_eval_test_target;

    for (i = 0; i < expr_vector.size (); ++i) {
        agfilter = false;
        jobid = 0;
        std::vector<std::pair<std::string, std::string>> predicates;
        rc = evaluator.extract (expr_vector.at (i), expr_eval_test_target, predicates);
        for (auto const &p : predicates) {
            if (p.first == "jobid-alloc") {
                jobid = std::stoul (p.second);
            } else if (p.first == "agfilter") {
                if (p.second == "true" || p.second == "t") {
                    agfilter = true;
                } else {
                    agfilter = false;
                }
            }
        }
        expected = must_success ? rc == 0 : rc < 0;
        expected_jobid = (jobids.at (i) == jobid);
        expected_agfilter = (agfilters.at (i) == agfilter);

        ok (expected && expected_jobid && expected_agfilter,
            "%s: ^%s$",
            label.c_str (),
            expr_vector.at (i).c_str ());
    }
}

int main (int argc, char *argv[])
{
    size_t ntests = 0;
    std::vector<std::string> expr_vector1;
    std::vector<std::string> expr_vector2;
    std::vector<std::string> expr_vector3;
    std::vector<std::string> expr_vector4;
    std::vector<unsigned long> extr_vector_jobids{1, 10, 10, 0};
    std::vector<bool> extr_vector_ag{false, false, true, true};

    build_simple_expr (expr_vector1);

    build_paren_expr (expr_vector2);

    build_invalid_expr (expr_vector3);

    build_extract_expr (expr_vector4);

    ntests = expr_vector1.size () + expr_vector2.size () + expr_vector3.size ();

    plan (2 * ntests + expr_vector4.size ());

    test_validation (expr_vector1, "validates simple expr", true);

    test_evaluation (expr_vector1, "evaluates simple expr", true);

    test_validation (expr_vector2, "validates paren expr", true);

    test_evaluation (expr_vector2, "evaluates paren expr", true);

    test_validation (expr_vector3, "invalidates malformed expr", false);

    test_evaluation (expr_vector3, "expectedly evaluates malformed", false);

    test_extraction (expr_vector4,
                     extr_vector_jobids,
                     extr_vector_ag,
                     "extracts as expected",
                     true);

    done_testing ();

    return EXIT_SUCCESS;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
