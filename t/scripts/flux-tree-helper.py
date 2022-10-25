#!/usr/bin/env python3

##############################################################
# Copyright 2020 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

import os
import sys
import time
import json
import argparse
import logging

import flux
import flux.util
import flux.kvs
import flux.job

LOGGER = logging.getLogger("flux-tree-helper")


def get_child_jobids(flux_handle, num_children, child_name):
    """
    Get the jobids of num_children instances.  Will repeatedly query the
    job-info module until num_children jobids are collected, with sleeps
    inbetween queries.
    """
    jobids = set()
    since = 0.0
    LOGGER.debug("Getting IDs of inactive children with name == %s", child_name)
    while True:
        for job in flux.job.job_list_inactive(
            flux_handle,
            max_entries=num_children,
            since=since,
            attrs=["t_inactive"],
            name=child_name,
        ).get_jobs():
            jobid = job["id"]
            since = max(since, job["t_inactive"])
            jobids.add(jobid)
        if len(jobids) >= num_children:
            break
        LOGGER.debug(
            "Only %d out of %d children are inactive, sleeping before trying again",
            len(jobids),
            num_children,
        )
        time.sleep(1)
    return jobids


def get_this_instance_data():
    data = json.load(sys.stdin)
    return data


def get_child_data(flux_handle, num_children, child_name, kvs_key):
    child_data = []
    jobids = get_child_jobids(flux_handle, num_children, child_name)
    for jobid in jobids:
        kvs_dir = flux.job.job_kvs_guest(flux_handle, jobid)
        child_data.append(kvs_dir[kvs_key])
    return child_data


def combine_data(this_instance_data, child_data):
    this_instance_data["child"] = child_data
    return this_instance_data


class PerfOutputFormat(flux.util.OutputFormat):
    """
    Store a parsed version of the program's output format,
    allowing the fields to iterated without modifiers, building
    a new format suitable for headers display, etc...
    """

    #  List of legal format fields and their header names
    headings = dict(
        treeid="TreeID",
        elapse="Elapsed(sec)",
        begin="Begin(Epoch)",
        end="End(Epoch)",
        match="Match(usec)",
        njobs="NJobs",
        my_nodes="NNodes",
        my_cores="CPN",
        my_gpus="GPN",
    )

    def __init__(self, fmt):
        """
        Parse the input format fmt with string.Formatter.
        Save off the fields and list of format tokens for later use,
        (converting None to "" in the process)

        Throws an exception if any format fields do not match the allowed
        list of headings above.
        """
        # Support both new and old style OutputFormat constructor:
        try:
            super().__init__(fmt, headings=self.headings, prepend="")
        except TypeError:
            super().__init__(PerfOutputFormat.headings, fmt)


def write_data_to_file(output_filename, output_format, data):
    def json_traverser(data):
        fieldnames = PerfOutputFormat.headings.keys()
        output = {k: v for k, v in data.items() if k in fieldnames}
        output.update(data["perf"])
        yield output
        for child in data["child"]:
            yield from json_traverser(child)

    formatter = PerfOutputFormat(output_format)
    with open(output_filename, "w") as outfile:
        header = formatter.header() + "\n"
        outfile.write(header)
        fmt = formatter.get_format() + "\n"
        for data_row in json_traverser(data):
            # newline = formatter.format(data_row)
            newline = fmt.format(**data_row)
            outfile.write(newline)


def write_data_to_parent(flux_handle, kvs_key, data):
    try:
        parent_uri = flux_handle.flux_attr_get("parent-uri")
    except FileNotFoundError:
        return
    parent_handle = flux.Flux(parent_uri)

    try:
        parent_kvs_namespace = flux_handle.flux_attr_get("parent-kvs-namespace").decode(
            "utf-8"
        )
    except FileNotFoundError:
        return
    env_name = "FLUX_KVS_NAMESPACE"
    os.environ[env_name] = parent_kvs_namespace

    flux.kvs.put(parent_handle, kvs_key, data)
    flux.kvs.commit(parent_handle)


def parse_args():
    parser = argparse.ArgumentParser(
        prog="flux-tree-helper", formatter_class=flux.util.help_formatter()
    )
    parser.add_argument(
        "num_children",
        type=int,
        help="number of children to collect data from.  Should be 0 at leaves.",
    )
    parser.add_argument(
        "kvs_key", type=str, help="key to use when propagating data up through the tree"
    )
    parser.add_argument(
        "job_name",
        type=str,
        help="name of the child jobs to use when filtering the inactive jobs",
    )
    parser.add_argument(
        "--perf-out",
        type=str,
        help="Dump the performance data into the given file. "
        "Assumed to be given at the root instance.",
    )
    parser.add_argument(
        "--perf-format",
        type=str,
        help="Dump the performance data with the given format string.",
    )
    return parser.parse_args()


@flux.util.CLIMain(LOGGER)
def main():
    args = parse_args()
    flux_handle = None
    try:
        flux_handle = flux.Flux()
    except FileNotFoundError:
        flux_handle = None

    LOGGER.debug("Getting this instance's data")
    this_data = get_this_instance_data()
    if flux_handle is not None and args.num_children > 0:
        LOGGER.debug("Getting children's data")
        child_data = get_child_data(
            flux_handle, args.num_children, args.job_name, args.kvs_key
        )
    else:
        child_data = []
    LOGGER.debug("Combining data")
    combined_data = combine_data(this_data, child_data)
    if flux_handle is not None:
        LOGGER.debug("Writing data to parent's KVS")
        write_data_to_parent(flux_handle, args.kvs_key, combined_data)
    if args.perf_out:
        LOGGER.debug("Writing data to file")
        write_data_to_file(args.perf_out, args.perf_format, combined_data)


if __name__ == "__main__":
    main()
