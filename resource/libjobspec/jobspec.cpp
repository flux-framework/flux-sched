/*****************************************************************************\
 * Copyright 2017 Lawrence Livermore National Security, LLC
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

#include "jobspec.hpp"
#include "resource/schema/data_std.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

using namespace Flux::Jobspec;
using namespace std::string_view_literals;

parse_error::parse_error (const char *msg)
    : runtime_error (msg), position (-1), line (-1), column (-1)
{
}

parse_error::parse_error (const YAML::Node &node, const char *msg)
    : runtime_error (msg),
#ifdef HAVE_YAML_MARK
      position (node.Mark ().pos),
      line (node.Mark ().line + 1),
      column (node.Mark ().column)
#else
      position (-1),
      line (-1),
      column (-1)
#endif
{
}

namespace {
void parse_yaml_count (Resource &res, const YAML::Node &cnode)
{
    /* count can have an unsigned integer value */
    if (cnode.IsScalar ()) {
        res.count.min = cnode.as<unsigned> ();
        res.count.max = res.count.min;
        return;
    }

    /* or count may be a more complicated verbose form */
    if (!cnode.IsMap ()) {
        throw parse_error (cnode, "count is not a mapping");
    }

    /* Verify existence of required entries */
    if (!cnode["min"]) {
        throw parse_error (cnode, "Key \"min\" missing from count");
    }
    if (!cnode["min"].IsScalar ()) {
        throw parse_error (cnode["min"], "Value of \"min\" must be a scalar");
    }
    if (cnode["max"] || cnode["operator"] || cnode["operand"]) {
        if (!cnode["max"] || !cnode["operator"] || !cnode["operand"]) {
            throw parse_error (cnode,
                               "All or none of \"max\", \"operator\", and \"operand\" must be "
                               "specified");
        }
        if (!cnode["max"].IsScalar ()) {
            throw parse_error (cnode["max"], "Value of \"max\" must be a scalar");
        }
        if (!cnode["operator"].IsScalar ()) {
            throw parse_error (cnode["operator"], "Value of \"operator\" must be a scalar");
        }
        if (!cnode["operand"].IsScalar ()) {
            throw parse_error (cnode["operand"], "Value of \"operand\" must be a scalar");
        }
        res.count.max = cnode["max"].as<unsigned> ();
        res.count.oper = cnode["operator"].as<char> ();
        res.count.operand = cnode["operand"].as<int> ();
    }

    /* Validate values of entries */
    res.count.min = cnode["min"].as<unsigned> ();
    if (cnode["min"].as<long long> () < 1) {
        throw parse_error (cnode["min"], "\"min\" must be greater than zero");
    }
    if (cnode["max"] && cnode["max"].as<long long> () < cnode["min"].as<long long> ()) {
        throw parse_error (cnode["max"], "\"max\" must be greater than or equal to \"min\"");
    }
    switch (res.count.oper) {
        case '+':
            if (res.count.operand < 1) {
                throw parse_error (cnode["operand"],
                                   "\"operand\" must be greater than zero for addition '+'");
            }
            break;
        case '*':
            if (res.count.operand < 2) {
                throw parse_error (cnode["operand"],
                                   "\"operand\" must be greater than one for multiplication '*'");
            }
            break;
        case '^':
            if (res.count.operand < 2) {
                throw parse_error (cnode["operand"],
                                   "\"operand\" must be greater than one for exponentiation '^'");
            }
            if (res.count.min < 2) {
                throw parse_error (cnode["min"],
                                   "\"min\" must be greater than one for exponentiation '^'");
            }
            break;
        default:
            throw parse_error (cnode["operator"], "Invalid count operator");
    }
}
}  // namespace

namespace {
std::vector<Resource> parse_yaml_resources (const YAML::Node &resources);
}

Resource::Resource (const YAML::Node &resnode)
{
    unsigned field_count = 0;

    /* The resource must be a mapping */
    if (!resnode.IsMap ()) {
        throw parse_error (resnode, "resource is not a mapping");
    }
    if (!resnode["type"]) {
        throw parse_error (resnode, "Key \"type\" missing from resource");
    }
    if (!resnode["type"].IsScalar ()) {
        throw parse_error (resnode["type"], "Value of \"type\" must be a scalar");
    }
    try {
        type = resource_model::resource_type_t{resnode["type"].as<std::string> ()};
    } catch (std::system_error e) {
        // resource_type must be closed or full
        throw parse_error (resnode["type"],
                           "Value of \"type\" must be a resource type known to fluxion");
    }
    field_count++;

    if (!resnode["count"]) {
        throw parse_error (resnode, "Key \"count\" missing from resource");
    }
    parse_yaml_count (*this, resnode["count"]);
    field_count++;

    if (resnode["unit"]) {
        if (!resnode["unit"].IsScalar ()) {
            throw parse_error (resnode["unit"], "Value of \"unit\" must be a scalar");
        }
        field_count++;
        unit = resnode["unit"].as<std::string> ();
    }
    if (resnode["exclusive"]) {
        if (!resnode["exclusive"].IsScalar ()) {
            throw parse_error (resnode["exclusive"], "Value of \"exclusive\" must be a scalar");
        }
        field_count++;
        std::string val = resnode["exclusive"].as<std::string> ();
        if (val == "false") {
            exclusive = tristate_t::FALSE;
        } else if (val == "true") {
            exclusive = tristate_t::TRUE;
        } else {
            throw parse_error (resnode["exclusive"],
                               "Value of \"exclusive\" must be either \"true\" or \"false\"");
        }
    }

    if (resnode["with"]) {
        field_count++;
        with = parse_yaml_resources (resnode["with"]);
    }

    if (resnode["label"]) {
        if (!resnode["label"].IsScalar ()) {
            throw parse_error (resnode["label"], "Value of \"label\" must be a scalar");
        }
        field_count++;
        label = resnode["label"].as<std::string> ();
    } else if (type == resource_model::slot_rt) {
        throw parse_error (resnode, "All slots must be labeled");
    }

    if (resnode["id"]) {
        if (!resnode["id"].IsScalar ()) {
            throw parse_error (resnode["id"], "Value of \"id\" must be a scalar");
        }
        field_count++;
        id = resnode["id"].as<std::string> ();
    }

    if (field_count != resnode.size ()) {
        throw parse_error (resnode, "Unrecognized key in resource mapping");
    }

    if (resnode.size () < 2 || resnode.size () > 10) {
        throw parse_error (resnode, "impossible number of entries in resource mapping");
    }
}

Task::Task (const YAML::Node &tasknode)
{
    /* The task node must be a mapping */
    if (!tasknode.IsMap ()) {
        throw parse_error (tasknode, "task is not a mapping");
    }
    if (!tasknode["command"]) {
        throw parse_error (tasknode, "Key \"command\" missing from task");
    }
    if (tasknode["command"].IsSequence ()) {
        command = tasknode["command"].as<std::vector<std::string>> ();
    } else {
        throw parse_error (tasknode["command"], "\"command\" value must be a sequence");
    }

    /* Import slot */
    if (!tasknode["slot"]) {
        throw parse_error (tasknode, "Key \"slot\" missing from task");
    }
    if (!tasknode["slot"].IsScalar ()) {
        throw parse_error (tasknode["slot"], "Value of task \"slot\" must be a YAML scalar");
    }
    slot = tasknode["slot"].as<std::string> ();

    /* Import count mapping */
    if (tasknode["count"]) {
        YAML::Node count_node = tasknode["count"];
        if (!count_node.IsMap ()) {
            throw parse_error (count_node, "\"count\" in task is not a mapping");
        }
        if (count_node.size () != 1) {
            throw parse_error (count_node, "\"count\" in task must have exactly one entry");
        }
        for (auto &&entry : count_node) {
            count[entry.first.as<std::string> ()] = entry.second.as<std::string> ();
        }
    }

    /* Import distribution if it is present */
    if (tasknode["distribution"]) {
        if (!tasknode["distribution"].IsScalar ()) {
            throw parse_error (tasknode["distribution"],
                               "Value of task \"distribution\" must be a YAML scalar");
        }
        distribution = tasknode["distribution"].as<std::string> ();
    }

    /* Import attributes mapping if it is present */
    if (tasknode["attributes"]) {
        YAML::Node attrs = tasknode["attributes"];
        if (!attrs.IsMap ()) {
            throw parse_error (attrs, "\"attributes\" in task is not a mapping");
        }
        for (auto &&attr : attrs) {
            attributes[attr.first.as<std::string> ()] = attr.second.as<std::string> ();
        }
    }

    if (tasknode.size () < 3 || tasknode.size () > 5) {
        throw parse_error (tasknode, "impossible number of entries in task mapping");
    }
}

namespace {
std::vector<Task> parse_yaml_tasks (const YAML::Node &tasks)
{
    std::vector<Task> taskvec;

    /* "tasks" must be a sequence */
    if (!tasks.IsSequence ()) {
        throw parse_error (tasks, "\"tasks\" is not a sequence");
    }

    for (auto &&task : tasks) {
        taskvec.push_back (Task (task));
    }

    return taskvec;
}
}  // namespace

namespace {
std::vector<Resource> parse_yaml_resources (const YAML::Node &resources)
{
    std::vector<Resource> resvec;

    /* "resources" must be a sequence */
    if (!resources.IsSequence ()) {
        throw parse_error (resources, "\"resources\" is not a sequence");
    }

    for (auto &&resource : resources) {
        resvec.push_back (Resource (resource));
    }

    return resvec;
}
}  // namespace

namespace {
Attributes parse_yaml_attributes (const YAML::Node &attrs)
{
    Attributes a;

    if (!attrs.IsMap ()) {
        throw parse_error (attrs, "\"attributes\" is not a map");
    }
    for (auto &&kv : attrs) {
        if (kv.first.as<std::string> () == "user") {
            a.user = kv.second;
        } else if (kv.first.as<std::string> () == "system") {
            for (auto &&s : kv.second) {
                if (s.first.as<std::string> () == "duration") {
                    a.system.duration = s.second.as<double> ();
                } else if (s.first.as<std::string> () == "queue") {
                    a.system.queue = s.second.as<std::string> ();
                } else if (s.first.as<std::string> () == "cwd") {
                    a.system.cwd = s.second.as<std::string> ();
                } else if (s.first.as<std::string> () == "environment") {
                    for (auto &&e : s.second) {
                        a.system.environment[e.first.as<std::string> ()] =
                            e.second.as<std::string> ();
                    }
                } else if (s.first.as<std::string> () == "constraints") {
                    a.system.constraint = constraint_parser (s.second);
                } else {
                    a.system.optional[s.first.as<std::string> ()] = s.second;
                }
            }
        } else {
            throw parse_error (kv.second, "Unknown key in \"attributes\"");
        }
    }
    return a;
}
}  // namespace

Jobspec::Jobspec (const YAML::Node &top)
{
    try {
        /* The top yaml node of the jobspec must be a mapping */
        if (!top.IsMap ()) {
            throw parse_error (top, "Top level of jobspec is not a mapping");
        }
        /* The four keys must be the following */
        if (!top["version"]) {
            throw parse_error (top, "Missing key \"version\" in top level mapping");
        }
        if (!top["resources"]) {
            throw parse_error (top, "Missing key \"resource\" in top level mapping");
        }
        if (!top["tasks"]) {
            throw parse_error (top, "Missing key \"tasks\" in top level mapping");
        }
        if (!top["attributes"]) {
            throw parse_error (top, "Missing key \"attributes\" in top level mapping");
        }
        /* There must be exactly four entries in the mapping */
        if (top.size () != 4) {
            throw parse_error (top, "Top mapping in jobspec must have exactly four entries");
        }

        /* Import version */
        if (!top["version"].IsScalar ()) {
            throw parse_error (top["version"], "\"version\" must be an unsigned integer");
        }
        version = top["version"].as<unsigned int> ();
        if (version < 1 || version > 9999) {
            throw parse_error (top["version"], "Only jobspec \"version\" 1-9999 is supported");
        }

        /* Import attributes mappings */
        if (!top["attributes"].IsNull ())
            attributes = parse_yaml_attributes (top["attributes"]);

        /* Import resources section */
        resources = parse_yaml_resources (top["resources"]);

        /* Import tasks section */
        tasks = parse_yaml_tasks (top["tasks"]);
    } catch (YAML::Exception &e) {
        throw parse_error (e.what ());
    }
}

Jobspec::Jobspec (std::istream &is)
try : Jobspec{YAML::Load (is)} {
} catch (YAML::Exception &e) {
    throw parse_error (e.what ());
}

Jobspec::Jobspec (const std::string &s)
try : Jobspec{YAML::Load (s)} {
} catch (YAML::Exception &e) {
    throw parse_error (e.what ());
}

namespace {
/*
 * This class magically makes everything in 'dest' stream
 * indented as long as this class is in scope.  Once it
 * it goes out of scope and is destroyed, the indenting
 * disappears.
 */
class IndentingOStreambuf : public std::streambuf {
    std::streambuf *myDest;
    bool myIsAtStartOfLine;
    std::string myIndent;
    std::ostream *myOwner;

   protected:
    virtual int overflow (int ch)
    {
        if (myIsAtStartOfLine && ch != '\n') {
            myDest->sputn (myIndent.data (), myIndent.size ());
        }
        myIsAtStartOfLine = ch == '\n';
        return myDest->sputc (ch);
    }

   public:
    explicit IndentingOStreambuf (std::streambuf *dest, int indent = 2)
        : myDest (dest), myIsAtStartOfLine (true), myIndent (indent, ' '), myOwner (NULL)
    {
    }
    explicit IndentingOStreambuf (std::ostream &dest, int indent = 2)
        : myDest (dest.rdbuf ()), myIsAtStartOfLine (true), myIndent (indent, ' '), myOwner (&dest)
    {
        myOwner->rdbuf (this);
    }
    virtual ~IndentingOStreambuf ()
    {
        if (myOwner != NULL) {
            myOwner->rdbuf (myDest);
        }
    }
};
}  // namespace

std::ostream &Flux::Jobspec::operator<< (std::ostream &s, Jobspec const &jobspec)
{
    s << "version: " << jobspec.version << std::endl;
    s << "resources: " << std::endl;
    for (auto &&resource : jobspec.resources) {
        IndentingOStreambuf indent (s);
        s << resource;
    }
    s << "tasks: " << std::endl;
    for (auto &&task : jobspec.tasks) {
        IndentingOStreambuf indent (s);
        s << task;
    }
    s << "attributes:" << std::endl;
    s << "  "
      << "system:" << std::endl;
    s << "    "
      << "duration: " << jobspec.attributes.system.duration << std::endl;
    s << "    "
      << "cwd: " << jobspec.attributes.system.cwd << std::endl;
    s << "    "
      << "queue: " << jobspec.attributes.system.queue << std::endl;
    s << "    "
      << "environment:" << std::endl;
    for (auto &&e : jobspec.attributes.system.environment) {
        s << "      " << e.first << ": " << e.second << std::endl;
    }
    s << "    "
      << "constraints:" << std::endl;
    if (jobspec.attributes.system.constraint != nullptr) {
        std::stringstream ss;
        std::string line;
        ss << jobspec.attributes.system.constraint->as_yaml ();
        while (std::getline (ss, line))
            s << "      " << line << std::endl;
    }
    return s;
}

std::ostream &Flux::Jobspec::operator<< (std::ostream &s, Resource const &resource)
{
    s << "- type: " << resource.type << std::endl;
    s << "  count:" << std::endl;
    s << "    min: " << resource.count.min << std::endl;
    s << "    max: " << resource.count.max << std::endl;
    s << "    operator: " << resource.count.oper << std::endl;
    s << "    operand: " << resource.count.operand << std::endl;
    if (resource.unit.size () > 0)
        s << "  unit: " << resource.unit << std::endl;
    if (resource.label.size () > 0)
        s << "  label: " << resource.label << std::endl;
    if (resource.id.size () > 0)
        s << "  id: " << resource.id << std::endl;
    if (resource.exclusive == tristate_t::TRUE)
        s << "  exclusive: true" << std::endl;
    else if (resource.exclusive == tristate_t::FALSE)
        s << "  exclusive: false" << std::endl;
    if (resource.with.size () > 0) {
        s << "  with:" << std::endl;
        IndentingOStreambuf indent (s, 4);
        for (auto &&child_resource : resource.with) {
            s << child_resource;
        }
    }

    return s;
}

std::ostream &Flux::Jobspec::operator<< (std::ostream &s, Task const &task)
{
    bool first = true;
    s << "command: [ ";
    for (auto &&field : task.command) {
        if (!first)
            s << ", ";
        else
            first = false;
        s << "\"" << field << "\"";
    }
    s << " ]" << std::endl;
    s << "slot: " << task.slot << std::endl;
    if (task.count.size () > 0) {
        s << "count:" << std::endl;
        IndentingOStreambuf indent (s);
        for (auto &&c : task.count) {
            s << c.first << ": " << c.second << std::endl;
        }
    }
    if (task.distribution.size () > 0)
        s << "distribution: " << task.distribution << std::endl;
    if (task.attributes.size () > 0) {
        s << "attributes:" << std::endl;
        IndentingOStreambuf indent (s);
        for (auto &&attr : task.attributes) {
            s << attr.first << ": " << attr.second;
        }
    }

    return s;
}
