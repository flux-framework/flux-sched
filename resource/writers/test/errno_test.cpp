/*****************************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
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

#include <cerrno>
#include <vector>
#include "resource/writers/match_writers.hpp"
#include "src/common/libtap/tap.h"

using namespace Flux::resource_model;

void test_compress_ids_errno ()
{
    sim_match_writers_t writer;
    std::stringstream out;
    std::vector<int64_t> invalid_ids = {1, -1, 3};  // Contains negative ID

    errno = 0;
    int rc = writer.compress_ids (out, invalid_ids);

    ok (rc == -1 && errno == EINVAL, "compress_ids returns -1 with errno=EINVAL for negative ID");

    // Test with valid IDs - should preserve errno
    errno = ENOENT;  // Set to non-zero to verify it's preserved
    std::vector<int64_t> valid_ids = {1, 2, 3};
    std::stringstream out2;
    rc = writer.compress_ids (out2, valid_ids);

    ok (rc == 0 && errno == ENOENT, "compress_ids returns 0 and preserves errno for valid IDs");
}

void test_compress_hosts_errno ()
{
    sim_match_writers_t writer;
    std::vector<std::string> hosts = {"host1", "host2"};

    errno = 0;
    int rc = writer.compress_hosts (hosts, nullptr, nullptr);

    ok (rc == -1 && errno == EINVAL,
        "compress_hosts returns -1 with errno=EINVAL for NULL hostlist_out");
}

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    test_compress_ids_errno ();
    test_compress_hosts_errno ();

    done_testing ();
    return EXIT_SUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
