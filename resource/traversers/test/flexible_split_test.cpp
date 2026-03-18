/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
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

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>
#include <catch2/catch_test_macros.hpp>

#include "resource/libjobspec/jobspec.hpp"
#include "resource/traversers/dfu_flexible.hpp"
#include "resource/schema/data_std.hpp"

using namespace Flux::Jobspec;
using namespace Flux::resource_model;

namespace {

std::string load_jobspec_file (const std::string &name)
{
    std::ifstream input (std::string{FLEXIBLE_JOBSPEC_DIR} + "/" + name);
    REQUIRE (input.is_open ());

    std::ostringstream buffer;
    buffer << input.rdbuf ();
    return buffer.str ();
}

bool contains_xor_slot (const std::vector<Resource> &resources)
{
    for (const auto &resource : resources) {
        if (resource.type == xor_slot_rt)
            return true;
        if (contains_xor_slot (resource.with))
            return true;
    }
    return false;
}

void collect_non_default_slot_labels (const std::vector<Resource> &resources,
                                      std::vector<std::string> &labels)
{
    for (const auto &resource : resources) {
        if (resource.type == slot_rt && resource.label != "default")
            labels.push_back (resource.label);
        collect_non_default_slot_labels (resource.with, labels);
    }
}

std::string resource_signature (const Resource &resource)
{
    std::ostringstream out;
    out << resource.type << "{" << resource.label << "," << resource.count.min << "}";

    if (!resource.with.empty ()) {
        out << "[";
        for (size_t i = 0; i < resource.with.size (); ++i) {
            if (i > 0)
                out << ";";
            out << resource_signature (resource.with[i]);
        }
        out << "]";
    }

    return out.str ();
}

std::string resource_list_signature (const std::vector<Resource> &resources)
{
    std::ostringstream out;
    for (size_t i = 0; i < resources.size (); ++i) {
        if (i > 0)
            out << "|";
        out << resource_signature (resources[i]);
    }
    return out.str ();
}

std::set<std::string> variant_signature_set (const std::vector<std::vector<Resource>> &variants)
{
    std::set<std::string> signatures;

    for (const auto &variant : variants)
        signatures.insert (resource_list_signature (variant));

    return signatures;
}

// Helper to create a Resource from YAML string
Resource make_resource (const std::string &yaml_str)
{
    YAML::Node node = YAML::Load (yaml_str);
    return Resource (node);
}

struct nested_expansion_expectations_t {
    size_t expected_variants;
    int expected_socket_branches;
    int expected_node_branches;
    int expected_cluster_branches;
    std::set<std::string> expected_labels;
};

// Use this helper for nested xor-slot jobspecs where each expanded variant
// should collapse to a single top-level branch (ie no cross product).
// Callers provide the expected total variant count plus the per-prefix branch
// counts and the full set of non-default slot labels that should appear across
// all expanded variants.
void check_nested_branch_expansion (const std::string &jobspec_file,
                                    const nested_expansion_expectations_t &expectations)
{
    Jobspec jobspec (load_jobspec_file (jobspec_file));
    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (jobspec.resources);

    int socket_branches = 0;
    int node_branches = 0;
    int cluster_branches = 0;
    std::set<std::string> labels;

    REQUIRE (variants.size () == expectations.expected_variants);
    for (const auto &variant : variants) {
        CHECK_FALSE (contains_xor_slot (variant));
        REQUIRE (variant.size () == 1);

        const auto &branch = variant.front ().with.front ();
        if (branch.type == socket_rt)
            ++socket_branches;
        else if (branch.type == node_rt)
            ++node_branches;
        else if (branch.type == cluster_rt)
            ++cluster_branches;

        std::vector<std::string> variant_labels;
        collect_non_default_slot_labels (variant, variant_labels);
        REQUIRE (variant_labels.size () == 1);
        labels.insert (variant_labels.front ());
    }

    CHECK (socket_branches == expectations.expected_socket_branches);
    CHECK (node_branches == expectations.expected_node_branches);
    CHECK (cluster_branches == expectations.expected_cluster_branches);
    CHECK (labels == expectations.expected_labels);
}

TEST_CASE ("dfu_flexible leaves non-xor jobspec files unchanged",
           "[resource][traversers][flexible]")
{
    constexpr std::array<const char *, 8> jobspec_files = {
        "test001.yaml",
        "test002.yaml",
        "test003.yaml",
        "test004.yaml",
        "test005.yaml",
        "test006.yaml",
        "test007.yaml",
        "test008.yaml",
    };

    for (const auto *jobspec_file : jobspec_files) {
        DYNAMIC_SECTION (jobspec_file)
        {
            Jobspec jobspec (load_jobspec_file (jobspec_file));
            dfu_flexible_t traverser;
            auto variants = traverser.split_xor_slots (jobspec.resources);

            REQUIRE (variants.size () == 1);
            CHECK_FALSE (contains_xor_slot (variants.front ()));
            CHECK (resource_list_signature (variants.front ())
                   == resource_list_signature (jobspec.resources));
        }
    }
}

TEST_CASE ("dfu_flexible expands top-level xor jobspec files into expected variants",
           "[resource][traversers][flexible]")
{
    struct xor_case_t {
        const char *jobspec_file;
        std::set<std::string> expected_signatures;
    };

    const std::array<xor_case_t, 4> cases = {{
        {
            "test009.yaml",
            {
                "slot{default,2}[core{,12};memory{,4}]",
                "slot{default,2}[core{,5};gpu{,1};memory{,4}]",
                "slot{default,2}[core{,4};gpu{,2};memory{,4}]",
            },
        },
        {
            "test010.yaml",
            {
                "cluster{,1}[rack{,1}[node{,1}[socket{,1}[slot{big,1}[core{,10}]]]]]",
                "cluster{,1}[rack{,1}[node{,1}[socket{,1}[slot{small,1}[core{,8};gpu{,1}]]]]]",
            },
        },
        {
            "test011.yaml",
            {
                "cluster{,1}[rack{,1}[node{,1}[socket{,1}[slot{small,1}[core{,8};gpu{,1}]]]]]",
            },
        },
        {
            "test013.yaml",
            {
                "cluster{,1}[rack{,1}[node{,1}[socket{,1}[slot{compact-branch,1}[core{,12};memory{,"
                "1}]]]]]",
                "cluster{,1}[rack{,1}[node{,1}[socket{,1}[slot{gpu-branch,1}[core{,8};gpu{,1}]]]]]",
                "cluster{,1}[rack{,1}[node{,1}[socket{,1}[slot{mem-branch,1}[core{,10};memory{,2}]]"
                "]]]",
            },
        },
    }};

    for (const auto &test_case : cases) {
        DYNAMIC_SECTION (test_case.jobspec_file)
        {
            Jobspec jobspec (load_jobspec_file (test_case.jobspec_file));
            dfu_flexible_t traverser;
            auto variants = traverser.split_xor_slots (jobspec.resources);

            for (const auto &variant : variants)
                CHECK_FALSE (contains_xor_slot (variant));

            CHECK (variant_signature_set (variants) == test_case.expected_signatures);
        }
    }
}

TEST_CASE ("dfu_flexible expands nested xor jobspec file test012",
           "[resource][traversers][flexible]")
{
    check_nested_branch_expansion ("test012.yaml",
                                   {9,
                                    3,
                                    3,
                                    3,
                                    {"a1", "a2", "a3", "b1", "b2", "b3", "c1", "c2", "c3"}});
}

TEST_CASE ("dfu_flexible expands nested xor branches", "[resource][traversers][flexible]")
{
    check_nested_branch_expansion ("test014.yaml",
                                   {9,
                                    3,
                                    3,
                                    3,
                                    {"a1", "a2", "a3", "b1", "b2", "b3", "c1", "c2", "c3"}});
}

TEST_CASE ("dfu_flexible expands xor cross products", "[resource][traversers][flexible]")
{
    Jobspec jobspec (load_jobspec_file ("test015.yaml"));
    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (jobspec.resources);
    std::set<std::string> combinations;

    REQUIRE (variants.size () == 4);
    for (const auto &variant : variants) {
        CHECK_FALSE (contains_xor_slot (variant));

        std::vector<std::string> labels;
        collect_non_default_slot_labels (variant, labels);
        std::sort (labels.begin (), labels.end ());
        REQUIRE (labels.size () == 2);
        combinations.insert (labels[0] + ":" + labels[1]);
    }

    CHECK (combinations == std::set<std::string>{"a1:s1", "a1:s2", "a2:s1", "a2:s2"});
}

TEST_CASE ("dfu_flexible handles empty xor_slot children", "[resource][traversers][flexible]")
{
    // Create a jobspec with an xor_slot that has no children
    auto xor_slot = make_resource (R"(
        type: xor_slot
        count: 1
        label: empty
    )");

    std::vector<Resource> resources = {xor_slot};

    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (resources);

    // An xor_slot with no children should produce a single variant
    // with a slot that has no children
    REQUIRE (variants.size () == 1);
    REQUIRE (variants[0].size () == 1);
    CHECK (variants[0][0].type == slot_rt);
    CHECK (variants[0][0].with.empty ());
}

TEST_CASE ("dfu_flexible handles single-option xor_slot", "[resource][traversers][flexible]")
{
    // Create a jobspec with an xor_slot that has only one option
    auto xor_slot = make_resource (R"(
        type: xor_slot
        count: 1
        label: default
        with:
          - type: core
            count: 4
    )");

    std::vector<Resource> resources = {xor_slot};

    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (resources);

    // Single option should produce one variant
    REQUIRE (variants.size () == 1);
    CHECK_FALSE (contains_xor_slot (variants[0]));
    REQUIRE (variants[0].size () == 1);
    CHECK (variants[0][0].type == slot_rt);
    REQUIRE (variants[0][0].with.size () == 1);
    CHECK (variants[0][0].with[0].type == core_rt);
}

TEST_CASE ("dfu_flexible handles mixed required and xor resources",
           "[resource][traversers][flexible]")
{
    // Create a jobspec with both required and xor_slot resources using inline YAML
    std::string yaml_str = R"(
        resources:
          - type: node
            count: 1
          - type: xor_slot
            count: 1
            label: default
            with:
              - type: core
                count: 4
    )";

    YAML::Node root = YAML::Load (yaml_str);
    std::vector<Resource> resources;
    for (const auto &res_node : root["resources"]) {
        resources.emplace_back (res_node);
    }

    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (resources);

    // Should produce one variant with node and the xor_slot converted to slot
    REQUIRE (variants.size () == 1);
    CHECK_FALSE (contains_xor_slot (variants[0]));
    REQUIRE (variants[0].size () == 2);

    // First resource should be node
    CHECK (variants[0][0].type == node_rt);

    // Second should be the slot
    CHECK (variants[0][1].type == slot_rt);
}

TEST_CASE ("dfu_flexible handles xor_slot with multiple siblings correctly",
           "[resource][traversers][flexible]")
{
    // Test: one required resource and two xor_slots should give 2 variants
    std::string yaml_str = R"(
        resources:
          - type: node
            count: 1
          - type: xor_slot
            count: 1
            label: x1
            with:
              - type: core
                count: 4
          - type: xor_slot
            count: 1
            label: x2
            with:
              - type: core
                count: 8
    )";

    YAML::Node root = YAML::Load (yaml_str);
    std::vector<Resource> resources;
    for (const auto &res_node : root["resources"]) {
        resources.emplace_back (res_node);
    }

    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (resources);

    // Should produce 2 variants (one for each xor option)
    REQUIRE (variants.size () == 2);

    for (const auto &variant : variants) {
        CHECK_FALSE (contains_xor_slot (variant));
        // Each variant should have node + one slot
        REQUIRE (variant.size () == 2);
        CHECK (variant[0].type == node_rt);
        CHECK (variant[1].type == slot_rt);
    }
}

TEST_CASE ("dfu_flexible rejects expansion exceeding limit", "[resource][traversers][flexible]")
{
    // Create a jobspec that would expand to more than SYSTEM_MAX_XOR_EXPANSION (1000) variants
    // Strategy: Create regular slot resources (not xor_slot) where each has xor_slot children.
    // Regular resources cross-multiply, so 10 slots with 2 xor_slot children each gives:
    // 2^10 = 1024 variants, which exceeds the 1000 limit.

    std::stringstream yaml_builder;
    yaml_builder << "resources:\n";

    // Create 10 regular slot resources, each with 2 xor_slot alternatives as children
    for (int i = 0; i < 10; ++i) {
        yaml_builder << "  - type: slot\n"
                     << "    count: 1\n"
                     << "    label: slot" << i << "\n"
                     << "    with:\n"
                     << "      - type: xor_slot\n"
                     << "        count: 1\n"
                     << "        label: opt_a\n"
                     << "        with:\n"
                     << "          - type: core\n"
                     << "            count: 1\n"
                     << "      - type: xor_slot\n"
                     << "        count: 1\n"
                     << "        label: opt_b\n"
                     << "        with:\n"
                     << "          - type: core\n"
                     << "            count: 2\n";
    }

    YAML::Node root = YAML::Load (yaml_builder.str ());
    std::vector<Resource> resources;
    for (const auto &res_node : root["resources"]) {
        resources.emplace_back (res_node);
    }

    dfu_flexible_t traverser;
    errno = 0;
    auto variants = traverser.split_xor_slots (resources);

    // Should fail with overflow: 10 slots × 2 xor options each = 2^10 = 1024 variants
    CHECK (variants.empty ());
    CHECK (errno == EOVERFLOW);
}

TEST_CASE ("dfu_flexible handles deeply nested xor_slots", "[resource][traversers][flexible]")
{
    // Build nested structure: xor_slot -> socket -> xor_slot -> core
    std::string yaml_str = R"(
        resources:
          - type: xor_slot
            count: 1
            label: outer
            with:
              - type: socket
                count: 1
                with:
                  - type: xor_slot
                    count: 1
                    label: inner
                    with:
                      - type: core
                        count: 4
    )";

    YAML::Node root = YAML::Load (yaml_str);
    std::vector<Resource> resources;
    for (const auto &res_node : root["resources"]) {
        resources.emplace_back (res_node);
    }

    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (resources);

    // Should successfully expand
    REQUIRE (variants.size () == 1);
    CHECK_FALSE (contains_xor_slot (variants[0]));

    // Verify structure: slot -> socket -> slot -> core
    REQUIRE (variants[0].size () == 1);
    CHECK (variants[0][0].type == slot_rt);
    REQUIRE (variants[0][0].with.size () == 1);
    CHECK (variants[0][0].with[0].type == socket_rt);
    REQUIRE (variants[0][0].with[0].with.size () == 1);
    CHECK (variants[0][0].with[0].with[0].type == slot_rt);
    REQUIRE (variants[0][0].with[0].with[0].with.size () == 1);
    CHECK (variants[0][0].with[0].with[0].with[0].type == core_rt);
}

TEST_CASE ("dfu_flexible handles empty resource list", "[resource][traversers][flexible]")
{
    std::vector<Resource> resources;

    dfu_flexible_t traverser;
    auto variants = traverser.split_xor_slots (resources);

    // Empty input should produce one empty variant
    REQUIRE (variants.size () == 1);
    CHECK (variants[0].empty ());
}

}  // namespace
