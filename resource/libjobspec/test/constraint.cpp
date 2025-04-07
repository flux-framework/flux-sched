/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
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

#include <iostream>
#include <string>
#include <map>
#include <yaml-cpp/yaml.h>

#include "resource/schema/resource_base.hpp"
#include "resource/libjobspec/constraint.hpp"
#include "src/common/libtap/tap.h"

// fake resource type for testing
struct resource : Flux::resource_model::resource_t {
    resource () {};
    void add_property (std::string name)
    {
        properties.insert (std::pair<std::string, std::string> (name, "t"));
    }
};

using namespace Flux;
using namespace Flux::Jobspec;

struct match_test {
    const char *desc;
    const char *json;
    bool result;
};

struct validate_test {
    const char *desc;
    const char *json;
    int rc;
    const char *err;
};

/*  These tests all assume a node object foo0 with properties xx and yy.
 */
struct match_test match_tests[] = {
    {"empty json object matches everything", "{}", true},
    {"empty properties dict matches everything", "{\"properties\": [] }", true},
    {
        "property matches",
        "{\"properties\": [\"xx\"]}",
        true,
    },
    {
        "logical not on property",
        "{\"properties\": [\"^xx\"]}",
        false,
    },
    {
        "logical not on unset property",
        "{\"properties\": [\"^zz\"]}",
        true,
    },
    {
        "property list matches like 'and'",
        "{\"properties\": [\"xx\", \"yy\"]}",
        true,
    },
    {
        "property list match fails unless node has all",
        "{\"properties\": [\"xx\", \"zz\"]}",
        false,
    },
    {
        "property list match fails if property missing",
        "{\"properties\": [\"zz\"]}",
        false,
    },
    {
        "and with two true statements",
        "{\"and\": [ {\"properties\": [\"xx\"]}, \
                   {\"properties\": [\"yy\"]}  \
                 ]}",
        true,
    },
    {
        "and with one false statement",
        "{\"and\": [ {\"properties\": [\"xx\"]}, \
                   {\"properties\": [\"zz\"]}  \
                 ]}",
        false,
    },
    {
        "or with two true statements",
        "{\"or\": [ {\"properties\": [\"xx\"]}, \
                  {\"properties\": [\"yy\"]}  \
                 ]}",
        true,
    },
    {
        "or with one true statements",
        "{\"or\": [ {\"properties\": [\"zz\"]}, \
                  {\"properties\": [\"yy\"]}  \
                 ]}",
        true,
    },
    {
        "or with two false statements",
        "{\"or\": [ {\"properties\": [\"zz\"]}, \
                  {\"properties\": [\"aa\"]}  \
                 ]}",
        false,
    },
    {
        "not with or with one true statement",
        "{\"not\": [ \
        {\"or\": [ {\"properties\": [\"zz\"]}, \
                   {\"properties\": [\"yy\"]}  \
                 ]} \
        ] \
       }",
        false,
    },
    {
        "hostlist operator works",
        "{\"hostlist\": [\"foo[0-2]\"]}",
        true,
    },
    {
        "hostlist operator works with non-matching hostlist",
        "{\"hostlist\": [\"foo[1-3]\"]}",
        false,
    },
    {
        "ranks operator works",
        "{\"ranks\": [\"0-2\"]}",
        true,
    },
    {
        "ranks operator works with non-intersecting ranks",
        "{\"ranks\": [\"1-3\"]}",
        false,
    },
    {NULL, NULL, false},
};

void test_match ()
{
    resource resource;
    resource.add_property ("xx");
    resource.add_property ("yy");
    resource.type = resource_model::node_rt;
    resource.name = "foo0";
    resource.basename = "foo";
    resource.rank = 0;

    struct match_test *t = match_tests;
    while (t->desc) {
        auto c = constraint_parser (YAML::Load (t->json));
        errno = 0;
        ok (c->match (resource) == t->result, "%s", t->desc);
        ok (errno == 0, "errno is preserved");
        t++;
    }
}

struct validate_test validate_tests[] =
    {{
         "non-object fails",
         "[]",
         -1,
         "constraint is not a mapping",
     },
     {
         "Unknown operation fails",
         "{ \"foo\": [] }",
         -1,
         "unknown constraint operator: foo",
     },
     {
         "multiple operations in one object fails",
         "{\"properties\": [\"xx\"], \"hostlist\": [\"foo\"]}",
         -1,
         "constraint map may not contain > 1 operation",
     },
     {
         "non-array argument to 'and' fails",
         "{ \"and\": \"foo\" }",
         -1,
         "and operator value must be an array",
     },
     {
         "non-array argument to 'or' fails",
         "{ \"or\": \"foo\" }",
         -1,
         "or operator value must be an array",
     },
     {
         "non-array argument to 'properties' fails",
         "{ \"properties\": \"foo\" }",
         -1,
         "properties operator value must be an array",
     },
     {
         "non-string property fails",
         "{ \"properties\": [ \"foo\", 42 ] }",
         -1,
         "non-string property specified",
     },
     {
         "invalid property string fails",
         "{ \"properties\": [ \"foo\", \"bar&\" ] }",
         -1,
         "bar& is invalid",
     },
     {"empty object is valid constraint", "{}", 0, NULL},
     {"empty and object is valid constraint", "{ \"and\": [] }", 0, NULL},
     {"empty or object is valid constraint", "{ \"or\": [] }", 0, NULL},
     {"empty properties object is valid constraint", "{ \"properties\": [] }", 0, NULL},
     {"complex conditional works",
      "{ \"and\": \
         [ { \"or\": \
             [ {\"properties\": [\"foo\"]}, \
               {\"properties\": [\"bar\"]}  \
             ] \
           }, \
           { \"and\": \
             [ {\"properties\": [\"xx\"]}, \
               {\"properties\": [\"yy\"]}  \
             ] \
           } \
         ] \
      }",
      0,
      NULL},
     {"hostlist can be included", "{\"hostlist\": [\"foo[0-10]\"]}", 0, NULL},
     {
         "invalid hostlist fails",
         "{\"hostlist\": [\"foo0-10]\"]}",
         -1,
         "Invalid hostlist `foo0-10]'",
     },
     {"ranks can be included", "{\"ranks\": [\"0-10\"]}", 0, NULL},
     {
         "invalid ranks entry fails",
         "{\"ranks\": [\"5,1-3\"]}",
         -1,
         "Invalid idset `5,1-3'",
     },
     {NULL, NULL, 0, NULL}};

void test_validate ()
{
    struct validate_test *t = validate_tests;
    while (t->desc) {
        bool exception = false;
        std::string errmsg;

        try {
            constraint_parser (YAML::Load (t->json));
        } catch (Flux::Jobspec::parse_error &e) {
            exception = true;
            errmsg = e.what ();
        }
        if (t->rc != 0) {
            ok (exception, "%s", t->desc);
            is (errmsg.c_str (), t->err, "%s: got expected error", t->desc);
        } else {
            ok (!exception, "%s", t->desc);
        }
        t++;
    }
}

int main (int ac, char *argv[])
{
    plan (NO_PLAN);
    test_match ();
    test_validate ();
    done_testing ();
    return 0;
}
