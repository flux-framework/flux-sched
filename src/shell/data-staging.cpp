/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/
#define FLUX_SHELL_PLUGIN_NAME "data-staging"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <flux/core.h>
#include <flux/shell.h>
#include <jansson.h>
}

#include <string>
#include <iostream>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include "src/common/libutil/json.hpp"

namespace fs = boost::filesystem;
using json = nlohmann::json;

enum class jobspec_state_t : int {
    INVALID = 0,
    VALID_NO_STAGING = 1,
    VALID_STAGING = 2
};

static jobspec_state_t validate_jobspec (json &jobspec)
{
    json storage_attrs;
    try {
        storage_attrs = \
            jobspec.at ("/attributes/system/data-staging"_json_pointer);
    } catch (json::out_of_range& e) {
        shell_debug ("Jobspec does not contain data-staging attributes. "
                   "No staging necessary.");
        return jobspec_state_t::VALID_NO_STAGING;
    }

    std::string req_attrs[] = {"label", "granularity", "stage-in"};
    for (const json &storage_attr : storage_attrs) {
        for (const auto &req_attr: req_attrs) {
            if (!storage_attr.contains (req_attr)) {
                shell_log_error ("Storage entry missing %s key",
                                 req_attr.c_str ());
                return jobspec_state_t::INVALID;
            }
        }

        json label = storage_attr.at ("label");
        if (!label.is_string ()) {
            shell_log_error ("Storage label must be a string");
            return jobspec_state_t::INVALID;
        }

        auto granularity = storage_attr.at ("granularity").get_ref<
            const std::string &>();
        if ((granularity != "node") && (granularity != "job")) {
            shell_log_error ("Storage entry contains a granularity not "
                             "equal to 'job' or 'node' (%s)",
                             granularity.c_str ());
            return jobspec_state_t::INVALID;
        }

        json stage_in = storage_attr.at ("stage-in");
        if (!stage_in.contains ("file")) {
            shell_log_error ("Stage-in entry missing file key");
            return jobspec_state_t::INVALID;
        } else if (stage_in.size () > 1) {
            shell_log_error ("Stage-in entry contains extra keys. "
                             "Only 'file' is valid.");
            return jobspec_state_t::INVALID;
        }
    }

    return jobspec_state_t::VALID_STAGING;
}

static int copy_data (const fs::path &source, const fs::path &destination)
{
    boost::system::error_code ec;
    fs::copy_file (source,
                   destination,
                   fs::copy_option::overwrite_if_exists,
                   ec);
    if (ec) {
        shell_log_error ("Error staging in data. Error code %d: %s",
                         ec.value (), ec.message ().c_str ());
        return -1;
    }
    shell_debug ("Completed staging in data.");
    return 0;
}

static boost::optional<fs::path> get_mountpoint (json &resource)
{
    // TODO: go from LinuxDeviceID in resource -> mountpoint in `resource`
    try {
        auto key = "/properties/mountpoint"_json_pointer;
        return fs::path (resource.at (key).get_ref<const std::string&>());
    } catch (json::exception& e) {
        return boost::none;
    }
}

static std::map<std::string, json> find_storage_resources (json &R, int rank)
{
    std::map<std::string, json> storage_resources;
    json jgf;
    try {
        jgf = R.at ("/scheduling/graph"_json_pointer);
    } catch (json::exception& e) {
        shell_log_error ("Data-staging plugin requires RV1 format, "
                         "with the JGF in the 'scheduling' key");
        return storage_resources;
    }

    try {
        for (json graph_node : jgf.at ("nodes")) {
            json metadata = graph_node.at ("metadata");
            if ((metadata.at ("rank") == rank) &&
                (metadata.at ("type") == "storage")) {
                auto key = "/ephemeral/label"_json_pointer;
                if (metadata.contains (key)) {
                    auto label = metadata.at (key).get<std::string>();
                    storage_resources.insert (std::make_pair (label, metadata));
                } else {
                    auto name = metadata.at ("name").get_ref<
                        const std::string&>();
                    shell_debug ("Storage resource in R without a label: %s",
                               name.c_str ());
                }
            }
        }
    } catch (json::exception& e) {
        shell_log_error ("Error parsing JGF in R");
        return storage_resources;
    }
    return storage_resources;
}

static int stage_data (json &jobspec, json &R, int rank)
{
    int rc = 0;
    auto data_staging_key = "/attributes/system/data-staging"_json_pointer;
    json data_staging_list = jobspec.at (data_staging_key);
    auto storage_resources = find_storage_resources (R, rank);
    shell_debug ("Found %d storage resources", (int) storage_resources.size ());

    for (const json &data_staging : data_staging_list) {
        // If the filesystem is job-level then only the first rank needs to do
        // the staging
        std::string granularity = data_staging.at ("granularity");
        if ((granularity == "job") && (rank > 0)) {
            shell_debug ("Rank %d skipping staging to a job granularity storage",
                       rank);
            continue;
        }

        auto source_key = "/stage-in/file"_json_pointer;
        auto source_str = data_staging.at (source_key).get_ref<
            const std::string&>();
        fs::path source {source_str};
        fs::path filename = source.filename();

        std::string attr_label = data_staging.at ("label");
        auto element = storage_resources.find (attr_label);
        if (element == storage_resources.end ()) {
            shell_log_error ("Missing storage resource in R with '%s' label",
                             attr_label.c_str ());
            return -1;
        }
        json resource = (*element).second;

        fs::path mountpoint;
        if (boost::optional<fs::path> m = get_mountpoint (resource)) {
             mountpoint = *m;
        } else {
            shell_log_error ("Resource in R missing mountpoint: %s",
                             resource.dump ().c_str ());
            return -1;
        }
        fs::path destination = mountpoint / filename;

        bool test = false;
        try {
            test = data_staging.at ("test").get<bool>();
             if (test) {
                shell_debug ("Rank %d staging from %s to %s",
                           rank,
                           source.c_str (),
                           destination.c_str ());
            }
        } catch (json::exception& e) {
            test = false;
        }

        if (!test)
            rc += copy_data (source, destination);
    }

    return rc;
}

static int main_init (flux_plugin_t *p)
{
    char *info_cstr = NULL;

    flux_shell_t *s = flux_plugin_get_shell (p);
    flux_plugin_set_name (p, FLUX_SHELL_PLUGIN_NAME);
    if (flux_shell_get_info (s, &info_cstr) < 0) {
        shell_log_error ("Error retrieving shell info string");
        errno = EINVAL;
        return -1;
    }

    json info;
    try {
        info = json::parse (info_cstr);
    } catch (json::parse_error& e) {
        shell_log_error ("Error parsing shell \"info\" json at byte %zu. %d: %s",
                         e.byte, e.id, e.what ());
        errno = EINVAL;
        return -1;
    }

    int rank;
    json jobspec, R;
    try {
        rank = info.at ("rank").get<int>();
        jobspec = info.at ("jobspec");
        R = info.at ("R");
    } catch (json::exception& e) {
        shell_log_error ("Error extracting keys from \"info\" json. %d: %s",
                         e.id, e.what ());
        errno = EINVAL;
        return -1;
    }

    jobspec_state_t jobspec_state = validate_jobspec (jobspec);
    switch (jobspec_state) {
    case jobspec_state_t::INVALID:
        errno=EINVAL;
        return -1;
    case jobspec_state_t::VALID_NO_STAGING:
        return 0;
    case jobspec_state_t::VALID_STAGING:
        break;
    }

    if (stage_data (jobspec, R, rank) < 0) {
        errno = EIO;
        return -1;
    }

    return 0;
}

extern "C" int flux_plugin_init (flux_plugin_t *p)
{
    try {
        return main_init (p);
    } catch (std::exception &e) {
        shell_log_error ("Uncaught exception occurred: %s", e.what ());
        return -1;
    }
}

