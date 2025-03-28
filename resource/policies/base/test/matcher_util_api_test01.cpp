/*****************************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
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

#include <jansson.h>
#include <limits>
#include <string>
#include "resource/policies/base/matcher.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux;
using namespace Flux::Jobspec;
using namespace Flux::resource_model;

////////////////////////////////////////////////////////////////////////////////
// Test Jobspec Template Class
////////////////////////////////////////////////////////////////////////////////

class test_jobspec_template {
   public:
    explicit test_jobspec_template (unsigned int min);
    explicit test_jobspec_template (unsigned int min,
                                    unsigned int max,
                                    std::string oper,
                                    unsigned int op);
    const std::string &get () const;

   private:
    int emit (json_t *count_obj);
    json_t *emit_count (unsigned int min);
    json_t *emit_count (unsigned int min,
                        unsigned int max,
                        const std::string &oper,
                        unsigned int op);

    std::string m_job_spec_string = "";
};

////////////////////////////////////////////////////////////////////////////////
// Private Test Jobspec Template Class Methods
////////////////////////////////////////////////////////////////////////////////

int test_jobspec_template::emit (json_t *count_obj)
{
    json_t *obj{nullptr};
    json_t *tasks_obj{nullptr};
    char *json_str{nullptr};

    if ((tasks_obj = json_pack ("[{s:[s] s:s s:{s:i}}]",
                                "command",
                                "app",
                                "slot",
                                "foo",
                                "count",
                                "per_slot",
                                1))
        == nullptr) {
        errno = ENOMEM;
        return -1;
    }

    if ((obj = json_pack ("{s:i s:[{s:s s:s s:o s:[{s:s s:i}]}] s:o s:{}}",
                          "version",
                          9999,
                          "resources",
                          "type",
                          "slot",
                          "label",
                          "foo",
                          "count",
                          count_obj,
                          "with",
                          "type",
                          "node",
                          "count",
                          1,
                          "tasks",
                          tasks_obj,
                          "attributes"))
        == nullptr) {
        errno = ENOMEM;
        return -1;
    }

    if ((json_str = json_dumps (obj, JSON_INDENT (0))) == nullptr) {
        errno = ENOMEM;
        return -1;
    }

    m_job_spec_string = json_str;
    json_decref (obj);
    free (json_str);
    return 0;
}

json_t *test_jobspec_template::emit_count (unsigned int min)
{
    return json_pack ("{s:i}", "min", min);
}

json_t *test_jobspec_template::emit_count (unsigned int min,
                                           unsigned int max,
                                           const std::string &oper,
                                           unsigned int op)
{
    return json_pack ("{s:i s:I s:s s:i}",
                      "min",
                      min,
                      "max",
                      (json_int_t)max,
                      "operator",
                      oper.c_str (),
                      "operand",
                      op);
}

////////////////////////////////////////////////////////////////////////////////
// Public Test Jobspec Template Class Methods
////////////////////////////////////////////////////////////////////////////////

test_jobspec_template::test_jobspec_template (unsigned int min)
{
    json_t *count_obj{nullptr};
    if ((count_obj = emit_count (min)) == nullptr)
        throw std::bad_alloc ();
    if ((emit (count_obj)) < 0)
        throw std::bad_alloc ();
}

test_jobspec_template::test_jobspec_template (unsigned int min,
                                              unsigned int max,
                                              std::string oper,
                                              unsigned int op)
{
    json_t *count_obj{nullptr};
    if ((count_obj = emit_count (min, max, oper, op)) == nullptr)
        throw std::bad_alloc ();
    if ((emit (count_obj)) < 0)
        throw std::bad_alloc ();
}

const std::string &test_jobspec_template::get () const
{
    return m_job_spec_string;
}

////////////////////////////////////////////////////////////////////////////////
// Test Routines
////////////////////////////////////////////////////////////////////////////////

void test_plus_op ()
{
    unsigned int count = 0;
    matcher_util_api_t api;

    // {min=1, max=10, operator='+', operand=1}
    test_jobspec_template jobin_001 (1, 10, "+", 1);
    Flux::Jobspec::Jobspec job_001{jobin_001.get ()};
    count = api.calc_count (job_001.resources[0], std::numeric_limits<unsigned int>::max ());
    ok (count == 10, "{min=1, max=10, oper='+', op=1} is 10");

    count = api.calc_count (job_001.resources[0], 5);
    ok (count == 5, "{min=1, max=10, oper='+', op=1} w/ qc=5 is 5");

    // {min=1, max=10, operator='+', operand=2}
    test_jobspec_template jobin_002 (1, 10, "+", 2);
    Flux::Jobspec::Jobspec job_002{jobin_002.get ()};

    count = api.calc_count (job_002.resources[0], std::numeric_limits<unsigned int>::max ());
    ok (count == 9, "{min=1, max=10, oper='+', op=2} is 9");

    count = api.calc_count (job_002.resources[0], 5);
    ok (count == 5, "{min=1, max=10, oper='+', op=2} w/ qc=5 is 5");

    // {min=1, max=10, operator='+', operand=3}
    test_jobspec_template jobin_003 (1, 10, "+", 3);
    Flux::Jobspec::Jobspec job_003{jobin_003.get ()};

    count = api.calc_count (job_003.resources[0], std::numeric_limits<unsigned int>::max ());
    ok (count == 10, "{min=1, max=10, oper='+', op=3} is 10");

    count = api.calc_count (job_003.resources[0], 5);
    ok (count == 4, "{min=1, max=10, oper='+', op=3} w/ qc=5 is 4");

    // {min=1, max=10, operator='+', operand=5}
    test_jobspec_template jobin_004 (1, 10, "+", 5);
    Flux::Jobspec::Jobspec job_004{jobin_004.get ()};

    count = api.calc_count (job_004.resources[0], std::numeric_limits<unsigned int>::max ());
    ok (count == 6, "{min=1, max=10, oper='+', op=5} is 6");

    count = api.calc_count (job_004.resources[0], 5);
    ok (count == 1, "{min=1, max=10, oper='+', op=5} w/ qc=5 is 1");

    // {min=1, max=10, operator='+', operand=10}
    test_jobspec_template jobin_005 (1, 10, "+", 10);
    Flux::Jobspec::Jobspec job_005{jobin_005.get ()};

    count = api.calc_count (job_005.resources[0], std::numeric_limits<unsigned int>::max ());
    ok (count == 1, "{min=1, max=10, oper='+', op=10} is 1");

    count = api.calc_count (job_005.resources[0], 5);
    ok (count == 1, "{min=1, max=10, oper='+', op=10} w/ qc=5 is 1");

    // {min=1, max=4294967295, operator='+', operand=1}
    test_jobspec_template jobin_006 (1, 4294967295, "+", 1);
    Flux::Jobspec::Jobspec job_006{jobin_006.get ()};

    count = api.calc_count (job_006.resources[0], std::numeric_limits<unsigned int>::max ());
    ok (count == 4294967295, "{min=1, max=4294967295, oper='+', op=1} is 4294967295");

    // {min=2, max=4294967295, operator='+', operand=2}
    test_jobspec_template jobin_007 (2, 4294967295, "+", 2);
    Flux::Jobspec::Jobspec job_007{jobin_007.get ()};

    count = api.calc_count (job_007.resources[0], std::numeric_limits<unsigned int>::max ());
    ok (count == 4294967294, "{min=2, max=4294967295, oper='+', op=2} is 4294967294");
}

int main (int argc, char *argv[])
{
    plan (12);

    test_plus_op ();

    done_testing ();

    return EXIT_SUCCESS;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
