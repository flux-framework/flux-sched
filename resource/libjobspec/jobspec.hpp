/*****************************************************************************\
 * Copyright 2017 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/*
 * This jobspec module handles parsing the Flux jobspec format as specified
 * in Spec 14 in the Flux RFC project: https://github.com/flux-framework/rfc
 *
 * The primary interface in the library is the Flux:Jobspec::Jobspec class.
 * The constructor Flux::Jobspec::Jobspec() can handle jobspec data in a
 * std::string, std::istream, or the top document YAML::Node as pre-processed
 * by the yaml-cpp library.
 *
 * When errors are found in the jobspec stream the library will raise the
 * Flux::Jobspec:parse_error exception.  If the library was able to determine
 * the location that the error occurred in jobspec yaml stream, it will appear
 * in the position, line, and column members of the parse_error object.  If it
 * is unable to determine the location, all three of those fields will be -1.
 *
 * NOTE: The library will only be able to determine the location of error with
 * yaml-cpp version 0.5.3 or newer.
 */

#ifndef JOBSPEC_HPP
#define JOBSPEC_HPP

#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <cstdint>
#include <limits>
#include <yaml-cpp/yaml.h>

#include "parse_error.hpp"
#include "constraint.hpp"
#include "resource/schema/data_std.hpp"

namespace Flux {
namespace Jobspec {

enum class tristate_t { FALSE, TRUE, UNSPECIFIED };

class Resource {
   public:
    resource_model::resource_type_t type;
    struct {
        unsigned min;
        unsigned max = std::numeric_limits<unsigned int>::max ();
        char oper = '+';
        int operand = 1;
    } count;
    std::string unit;
    std::string label;
    std::string id;
    tristate_t exclusive = tristate_t::UNSPECIFIED;
    std::vector<Resource> with;

    // user_data has no library internal usage, it is
    // entirely for the convenience of external code
    std::unordered_map<resource_model::resource_type_t, int64_t> user_data;
    bool cosched = false;
    unsigned cosched_count = 0;
    Resource (const YAML::Node &);
};

class Task {
   public:
    std::vector<std::string> command;
    std::string slot;
    std::unordered_map<std::string, std::string> count;
    std::string distribution;
    std::unordered_map<std::string, std::string> attributes;

    Task (const YAML::Node &);
};

struct System {
    double duration = 0.0f;
    bool cosched = false;
    bool c_r = false;  // Job Support Checkpoint/Restart
    std::string queue = "";
    std::string cwd = "";

    std::unordered_map<std::string, std::string> environment;
    std::unordered_map<std::string, YAML::Node> optional;
    std::shared_ptr<Constraint> constraint = nullptr;

    System () = default;
    System (const System &s) = delete;  // Force to use move ctor
    System (System &&s) = default;
    System &operator= (const System &&a) = delete;  // Force to use move operator=
    System &operator= (System &&a) = default;
};

struct Attributes {
    YAML::Node user;
    System system;

    Attributes () = default;
    Attributes (const Attributes &a) = delete;  // Force to use move ctor
    Attributes (Attributes &&a) = default;
    Attributes &operator= (const Attributes &&a) = delete;
    Attributes &operator= (Attributes &&a) = default;
};

class Jobspec {
   public:
    unsigned int version;
    std::vector<Resource> resources;
    std::vector<Task> tasks;
    Attributes attributes;

    Jobspec () = default;
    Jobspec (const YAML::Node &);
    Jobspec (std::istream &is);
    Jobspec (const std::string &s);
};

std::ostream &operator<< (std::ostream &s, Jobspec const &js);
std::ostream &operator<< (std::ostream &s, Resource const &r);
std::ostream &operator<< (std::ostream &s, Task const &t);

}  // namespace Jobspec
}  // namespace Flux

#endif  // JOBSPEC_HPP
