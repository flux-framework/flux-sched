/*****************************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

#include <iostream>
#include <filesystem>
#include <getopt.h>
#include "resource/readers/resource_spec_grug.hpp"

using namespace Flux::resource_model;

#define OPTIONS "hm"
static const struct option longopts[] = {
    {"more", no_argument, 0, 'm'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0},
};

void usage (int code)
{
    std::cerr << "Usage: grug2dot <genspec>.graphml\n"
                 "    Convert a resource-graph generator spec (<genspec>.graphml)\n"
                 "    to AT&T GraphViz format (<genspec>.dot). The output\n"
                 "    file only contains the basic information unless --more is given.\n"
                 "\n"
                 "    OPTIONS:\n"
                 "    -h, --help\n"
                 "            Display this usage information\n"
                 "\n"
                 "    -m, --more\n"
                 "            More information in the output file\n"
                 "\n";
    exit (code);
}

int main (int argc, char *argv[])
{
    int ch;
    int rc = 0;
    bool simple = true;
    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage (0);
                break;
            case 'm': /* --more */
                simple = false;
                break;
            default:
                usage (1);
                break;
        }
    }

    if (optind != (argc - 1))
        usage (1);

    resource_gen_spec_t gspec;
    std::string fn (argv[optind]);
    std::filesystem::path path = fn;
    std::string base = path.stem ().string ();

    if (gspec.read_graphml (fn) != 0) {
        std::cerr << "Error in reading " << fn << std::endl;
        rc = -1;
    } else if (gspec.write_graphviz (base + ".dot", simple) != 0) {
        std::cerr << "Error in writing " << base + ".dot" << std::endl;
        rc = -1;
    }

    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
