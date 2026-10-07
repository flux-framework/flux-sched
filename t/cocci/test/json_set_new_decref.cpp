// SPDX-License-Identifier: LGPL-3.0
//
// Known-bad test input for t/cocci/json_set_new_decref.cocci.
//
// Fluxion is C++, and spatch only makes "a small attempt" to parse C++.
// When that attempt fails, spatch silently matches nothing and exits 0,
// which would turn the coccinelle CI check into a no-op that always
// passes.  The CI job therefore runs each semantic patch against the
// matching file here first and fails if it reports no match.
//
// The functions below reproduce, in their original C++ context, the
// double-decref bugs fixed by "resource: fix double-json-decref".  This
// file is never compiled -- keep the bugs in it.

#include <jansson.h>

#include <algorithm>
#include <cerrno>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Reduced from get_property_request_cb () in resource/modules/resource.cpp.
static int values_to_json (const std::vector<std::string> &resp_values, json_t **result)
{
    json_t *resp_array = nullptr;

    if (!(resp_array = json_array ())) {
        errno = ENOMEM;
        goto error;
    }
    for (auto &resp_value : resp_values) {
        json_t *value = nullptr;
        if (!(value = json_string (resp_value.c_str ()))) {
            errno = EINVAL;
            goto error;
        }
        if (json_array_append_new (resp_array, value) < 0) {
            json_decref (value);  // BUG: json_array_append_new () stole value
            errno = EINVAL;
            goto error;
        }
    }
    *result = resp_array;
    return 0;

error:
    json_decref (resp_array);
    return -1;
}

// Reduced from rlite_match_writers_t::fill () in
// resource/writers/match_writers.cpp.
class prop_gatherer_t {
   public:
    int fill (json_t *props);

   private:
    std::map<std::string, std::vector<int64_t>> m_gl_prop_gatherer;
};

int prop_gatherer_t::fill (json_t *props)
{
    int rc = 0;

    for (auto &kv : m_gl_prop_gatherer) {
        std::stringstream s;
        json_t *ranks = nullptr;

        std::sort (kv.second.begin (), kv.second.end ());
        for (auto &rank : kv.second)
            s << rank << ",";
        if (!(ranks = json_string (s.str ().c_str ()))) {
            rc = -1;
            errno = EINVAL;
            goto ret;
        }
        if (json_object_set_new (props, kv.first.c_str (), ranks) < 0) {
            json_decref (ranks);  // BUG: json_object_set_new () stole ranks
            rc = -1;
            errno = EINVAL;
            goto ret;
        }
    }

ret:
    return rc;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
