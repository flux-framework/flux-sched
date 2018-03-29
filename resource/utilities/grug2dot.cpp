/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#include <iostream>
#include <getopt.h>
#include "resource/resource_gen_spec.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace std;
using namespace Flux::resource_model;

#define OPTIONS "hm"
static const struct option longopts[] = {
    {"more",      no_argument,  0, 'm'},
    {"help",      no_argument,  0, 'h'},
    { 0, 0, 0, 0 },
};

void usage (int code)
{
    cerr <<
"Usage: grug2dot <genspec>.graphml\n"
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
    string fn (argv[optind]);
    boost::filesystem::path path = fn;
    string base = path.stem ().string ();

    if (gspec.read_graphml (fn) != 0) {
        cerr << "Error in reading " << fn << endl;
        rc = -1;
    } else if (gspec.write_graphviz (base + ".dot", simple) != 0) {
        cerr << "Error in writing " << base + ".dot" << endl;
        rc = -1;
    }

    return rc;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
