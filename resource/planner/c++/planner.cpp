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
#include "config.h"
#endif
}

#include <limits>
#include <map>
#include <string>

#include "planner.hpp"

////////////////////////////////////////////////////////////////////////////////
// Public Span_t Methods
////////////////////////////////////////////////////////////////////////////////

bool span_t::operator== (const span_t &o) const
{
    if (start != o.start)
        return false;
    if (last != o.last)
        return false;
    if (span_id != o.span_id)
        return false;
    if (planned != o.planned)
        return false;
    if (in_system != o.in_system)
        return false;
    if ((*(start_p) != *(o.start_p)))
        return false;
    if ((*(last_p) != *(o.last_p)))
        return false;

    return true;
}

bool span_t::operator!= (const span_t &o) const
{
    return !operator== (o);
}

////////////////////////////////////////////////////////////////////////////////
// Public Planner Methods
////////////////////////////////////////////////////////////////////////////////

planner::planner () = default;

planner::planner (const int64_t base_time,
                  const uint64_t duration,
                  const uint64_t resource_totals,
                  const char *in_resource_type)
{
    m_total_resources = static_cast<int64_t> (resource_totals);
    m_resource_type = in_resource_type;
    m_plan_start = base_time;
    m_plan_end = base_time + static_cast<int64_t> (duration);
    m_p0 = new scheduled_point_t ();
    m_p0->in_mt_resource_tree = 0;
    m_p0->new_point = 1;
    m_p0->at = base_time;
    m_p0->ref_count = 1;
    m_p0->remaining = m_total_resources;
    m_sched_point_tree.insert (m_p0);
    m_mt_resource_tree.insert (m_p0);
    m_avail_time_iter_set = 0;
    m_span_counter = 0;
}

planner::planner (const planner &o)
{
    int rc = -1;
    // Important: need to copy trees first,
    // since map copies fetch the scheduled
    // points inserted into the trees.
    if ((rc = copy_trees (o)) < 0) {
        throw std::runtime_error ("ERROR copying trees\n");
    }
    if ((rc = copy_maps (o)) < 0) {
        throw std::runtime_error ("ERROR copying maps\n");
    }

    m_total_resources = o.m_total_resources;
    m_resource_type = o.m_resource_type;
    m_plan_start = o.m_plan_start;
    m_plan_end = o.m_plan_end;
    m_current_request = o.m_current_request;
    m_avail_time_iter_set = o.m_avail_time_iter_set;
    m_span_counter = o.m_span_counter;
    // After above copy the SP tree now has the full state of the base point.
    m_p0 = m_sched_point_tree.get_state (m_plan_start);
}

planner &planner::operator= (const planner &o)
{
    int rc = -1;

    if ((rc = erase ()) != 0) {
        throw std::runtime_error ("ERROR erasing *this\n");
    }
    // Important: need to copy trees first,
    // since map copies fetch the scheduled
    // points inserted into the trees.
    if ((rc = copy_trees (o)) != 0) {
        throw std::runtime_error ("ERROR copying trees to *this\n");
    }
    if ((rc = copy_maps (o)) != 0) {
        throw std::runtime_error ("ERROR copying maps to *this\n");
    }

    m_total_resources = o.m_total_resources;
    m_resource_type = o.m_resource_type;
    m_plan_start = o.m_plan_start;
    m_plan_end = o.m_plan_end;
    m_current_request = o.m_current_request;
    m_avail_time_iter_set = o.m_avail_time_iter_set;
    m_span_counter = o.m_span_counter;
    // After above copy the SP tree now has the full state of the base point.
    m_p0 = m_sched_point_tree.get_state (m_plan_start);

    return *this;
}

bool planner::operator== (const planner &o) const
{
    if (m_total_resources != o.m_total_resources)
        return false;
    if (m_resource_type != o.m_resource_type)
        return false;
    if (m_plan_start != o.m_plan_start)
        return false;
    if (m_plan_end != o.m_plan_end)
        return false;
    if (m_avail_time_iter_set != o.m_avail_time_iter_set)
        return false;
    if (m_span_counter != o.m_span_counter)
        return false;
    // m_p0 or o.m_p0 could be uninitialized
    if (m_p0 && o.m_p0) {
        if (*m_p0 != *(o.m_p0))
            return false;
    } else if (m_p0 || o.m_p0) {
        return false;
    }  // else both nullptr
    if (!span_lookups_equal (o))
        return false;
    if (!avail_time_iters_equal (o))
        return false;
    if (!trees_equal (o))
        return false;
    return true;
}

bool planner::operator!= (const planner &o) const
{
    return !operator== (o);
}

planner::~planner ()
{
    // Destructor is nothrow
    // The destroy function called in the
    // scheduled point tree deletes all scheduled
    // points (including m_p0)
    // inserted in the class ctor
    // or reinitialize.
    erase ();
}

int planner::erase ()
{
    int rc = 0;

    // returns 0 or a negative number
    rc = restore_track_points ();
    m_span_lookup.clear ();
    if (m_p0 && m_p0->in_mt_resource_tree)
        rc += m_mt_resource_tree.remove (m_p0);
    m_sched_point_tree.destroy ();
    m_mt_resource_tree.clear ();

    return rc;
}

int planner::reinitialize (int64_t base_time, uint64_t duration)
{
    int rc = 0;

    m_plan_start = base_time;
    m_plan_end = base_time + static_cast<int64_t> (duration);
    m_p0 = new scheduled_point_t ();
    m_p0->at = base_time;
    m_p0->ref_count = 1;
    m_p0->remaining = m_total_resources;
    // returns 0 or -1
    rc = m_sched_point_tree.insert (m_p0);
    // returns 0 or -1
    rc += m_mt_resource_tree.insert (m_p0);
    m_avail_time_iter_set = 0;
    m_span_counter = 0;

    return rc;
}

int planner::restore_track_points ()
{
    int rc = 0;

    if (!m_avail_time_iter.empty ()) {
        for (auto &kv : m_avail_time_iter) {
            // returns 0 or -1
            rc += m_mt_resource_tree.insert (kv.second);
        }
    }
    m_avail_time_iter.clear ();

    return rc;
}

int planner::update_total (uint64_t resource_total)
{
    int64_t delta = resource_total - m_total_resources;
    int64_t tmp = 0;
    scheduled_point_t *point = nullptr;
    if (delta == 0)
        return 0;
    m_total_resources = static_cast<int64_t> (resource_total);
    point = m_sched_point_tree.get_state (m_plan_start);
    while (point) {
        // Prevent remaining from taking negative values. This should
        // reduce likelihood of errors when adding and removing spans.
        // If the performance penalty is non-negligible we can
        // investigate letting remaining take negative values.
        tmp = point->remaining + delta;
        if (tmp >= 0)
            point->remaining = tmp;
        else
            point->remaining = 0;
        point = m_sched_point_tree.next (point);
    }
    return 0;
}

int64_t planner::get_total_resources () const
{
    return m_total_resources;
}

const std::string &planner::get_resource_type () const
{
    return m_resource_type;
}

int64_t planner::get_plan_start () const
{
    return m_plan_start;
}

int64_t planner::get_plan_end () const
{
    return m_plan_end;
}

int planner::mt_tree_insert (scheduled_point_t *point)
{
    return m_mt_resource_tree.insert (point);
}

int planner::mt_tree_remove (scheduled_point_t *point)
{
    return m_mt_resource_tree.remove (point);
}

int planner::sp_tree_insert (scheduled_point_t *point)
{
    return m_sched_point_tree.insert (point);
}

int planner::sp_tree_remove (scheduled_point_t *point)
{
    return m_sched_point_tree.remove (point);
}

void planner::destroy_sp_tree ()
{
    m_sched_point_tree.destroy ();
}

scheduled_point_t *planner::sp_tree_search (int64_t at)
{
    return m_sched_point_tree.search (at);
}

scheduled_point_t *planner::sp_tree_get_state (int64_t at)
{
    return m_sched_point_tree.get_state (at);
}

scheduled_point_t *planner::sp_tree_next (scheduled_point_t *point) const
{
    return m_sched_point_tree.next (point);
}

scheduled_point_t *planner::mt_tree_get_mintime (int64_t request) const
{
    return m_mt_resource_tree.get_mintime (request);
}

void planner::clear_span_lookup ()
{
    m_span_lookup.clear ();
}

void planner::span_lookup_erase (std::map<int64_t, std::shared_ptr<span_t>>::iterator &it)
{
    m_span_lookup.erase (it);
}

const std::map<int64_t, std::shared_ptr<span_t>> &planner::get_span_lookup_const () const
{
    return m_span_lookup;
}

std::map<int64_t, std::shared_ptr<span_t>> &planner::get_span_lookup ()
{
    return m_span_lookup;
}

size_t planner::span_lookup_get_size () const
{
    return m_span_lookup.size ();
}

void planner::span_lookup_insert (int64_t span_id, std::shared_ptr<span_t> span)
{
    m_span_lookup.insert (std::pair<int64_t, std::shared_ptr<span_t>> (span_id, span));
}

void planner::set_span_lookup_iter (std::map<int64_t, std::shared_ptr<span_t>>::iterator &it)
{
    m_span_lookup_iter = it;
}

const std::map<int64_t, std::shared_ptr<span_t>>::iterator planner::get_span_lookup_iter () const
{
    return m_span_lookup_iter;
}

void planner::incr_span_lookup_iter ()
{
    m_span_lookup_iter++;
}

std::map<int64_t, scheduled_point_t *> &planner::get_avail_time_iter ()
{
    return m_avail_time_iter;
}

const std::map<int64_t, scheduled_point_t *> &planner::get_avail_time_iter_const () const
{
    return m_avail_time_iter;
}

void planner::clear_avail_time_iter ()
{
    m_avail_time_iter.clear ();
}

void planner::set_avail_time_iter_set (int atime_iter_set)
{
    m_avail_time_iter_set = atime_iter_set;
}

const int planner::get_avail_time_iter_set () const
{
    return m_avail_time_iter_set;
}

request_t &planner::get_current_request ()
{
    return m_current_request;
}

const request_t &planner::get_current_request_const () const
{
    return m_current_request;
}

void planner::incr_span_counter ()
{
    m_span_counter++;
}

const uint64_t planner::get_span_counter () const
{
    return m_span_counter;
}

////////////////////////////////////////////////////////////////////////////////
// Private Planner Methods
////////////////////////////////////////////////////////////////////////////////

int planner::copy_trees (const planner &o)
{
    int rc = 0;

    // Incoming planner could be empty.
    if (!o.m_sched_point_tree.empty ()) {
        // Need to get the state at plan_start, not just m_p0 since they could
        // have diverged after scheduling.
        scheduled_point_t *point = o.m_sched_point_tree.get_state (o.m_plan_start);
        scheduled_point_t *new_point = nullptr;
        while (point) {
            new_point = new scheduled_point_t ();
            new_point->at = point->at;
            new_point->in_mt_resource_tree = point->in_mt_resource_tree;
            new_point->new_point = point->new_point;
            new_point->ref_count = point->ref_count;
            new_point->scheduled = point->scheduled;
            new_point->remaining = point->remaining;
            if ((rc = m_sched_point_tree.insert (new_point)) != 0)
                return rc;
            if ((rc = m_mt_resource_tree.insert (new_point)) != 0)
                return rc;
            point = o.m_sched_point_tree.next (point);
        }
    } else {
        // Erase if incoming planner is empty.
        rc = erase ();
    }
    return rc;
}

int planner::copy_maps (const planner &o)
{
    int rc = 0;

    if (!o.m_span_lookup.empty ()) {
        for (auto const &span_it : o.m_span_lookup) {
            std::shared_ptr<span_t> new_span = std::make_shared<span_t> ();
            // Need to deep copy spans, else copies of planners will share
            // pointers.
            new_span->start = span_it.second->start;
            new_span->last = span_it.second->last;
            new_span->span_id = span_it.second->span_id;
            new_span->planned = span_it.second->planned;
            new_span->in_system = span_it.second->in_system;
            new_span->start_p = m_sched_point_tree.get_state (new_span->start);
            new_span->last_p = m_sched_point_tree.get_state (new_span->last);
            m_span_lookup[span_it.first] = new_span;
        }
    } else {
        m_span_lookup.clear ();
    }
    if (!o.m_avail_time_iter.empty ()) {
        for (auto const &avail_it : o.m_avail_time_iter) {
            // Scheduled point already copied into trees, so fetch
            // from SP tree.
            scheduled_point_t *new_avail = m_sched_point_tree.get_state (avail_it.second->at);
            m_avail_time_iter[avail_it.first] = new_avail;
        }
    } else {
        m_avail_time_iter.clear ();
    }

    return rc;
}

bool planner::span_lookups_equal (const planner &o) const
{
    if (m_span_lookup.size () != o.m_span_lookup.size ())
        return false;
    if (!m_span_lookup.empty ()) {
        // Iterate through indices to use the auto range-based for loop
        // semantics; otherwise need to create a "zip" function for two maps.
        for (auto const &this_it : m_span_lookup) {
            auto const other = o.m_span_lookup.find (this_it.first);
            if (other == o.m_span_lookup.end ())
                return false;
            if (this_it.first != other->first)
                return false;
            // Compare span_t
            if (*(this_it.second) != *(other->second))
                return false;
        }
    }
    return true;
}

bool planner::avail_time_iters_equal (const planner &o) const
{
    if (m_avail_time_iter.size () != o.m_avail_time_iter.size ())
        return false;
    if (!m_avail_time_iter.empty ()) {
        // Iterate through indices to use the auto range-based for loop
        // semantics; otherwise need to create a "zip" function for two maps.
        for (auto const &this_it : m_avail_time_iter) {
            auto const other = o.m_avail_time_iter.find (this_it.first);
            if (other == o.m_avail_time_iter.end ())
                return false;
            if (this_it.first != other->first)
                return false;
            if (this_it.second && other->second) {
                if (*(this_it.second) != *(other->second))
                    return false;
            } else if (this_it.second || other->second) {
                return false;
            }
        }
    }
    return true;
}

bool planner::trees_equal (const planner &o) const
{
    if (m_sched_point_tree.get_size () != o.m_sched_point_tree.get_size ())
        return false;
    if (!m_sched_point_tree.empty ()) {
        scheduled_point_t *this_pt = m_sched_point_tree.get_state (m_plan_start);
        scheduled_point_t *o_pt = o.m_sched_point_tree.get_state (o.m_plan_start);
        while (this_pt) {
            if (*this_pt != *o_pt)
                return false;
            this_pt = m_sched_point_tree.next (this_pt);
            o_pt = o.m_sched_point_tree.next (o_pt);
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Public Planner_t methods
////////////////////////////////////////////////////////////////////////////////

planner_t::planner_t ()
{
    try {
        plan = new planner ();
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
}

planner_t::planner_t (const planner &o)
{
    try {
        plan = new planner (o);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
}

planner_t::planner_t (const int64_t base_time,
                      const uint64_t duration,
                      const uint64_t resource_totals,
                      const char *in_resource_type)
{
    try {
        plan = new planner (base_time, duration, resource_totals, in_resource_type);
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
    }
}

planner_t::~planner_t ()
{
    delete plan;
}

/*
 * vi: ts=4 sw=4 expandtab
 */
