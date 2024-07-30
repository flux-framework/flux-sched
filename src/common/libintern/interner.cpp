/*****************************************************************************\
* Copyright 2024 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "interner.hpp"

#include <assert.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace intern {
namespace detail {
std::shared_mutex group_mtx;

static std::size_t hash_combine (std::size_t lhs, std::size_t rhs)
{
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}

struct string_hash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator() (const char *str) const
    {
        return hash_type{}(str);
    }
    std::size_t operator() (std::string_view str) const
    {
        return hash_type{}(str);
    }
    std::size_t operator() (std::string const &str) const
    {
        return hash_type{}(str);
    }
};

struct interner_storage {
    std::vector<const std::string *> strings_by_id;
    // Tombstone is UINTPTR_MAX
    std::unordered_map<std::string, uintptr_t, string_hash, std::equal_to<> > ids_by_string;
    // reader/writer lock to protect entries
    std::unique_ptr<std::shared_mutex> mtx = std::make_unique<std::shared_mutex> ();
};

interner_storage &get_group (uintptr_t id)
{
    static std::unordered_map<uintptr_t, interner_storage> groups;
    auto guard = std::shared_lock (group_mtx);
    return groups[id];
}

static bool check_valid (uintptr_t val, char bytes_supported)
{
    if (bytes_supported == 1) {
        return val < UINT8_MAX;
    }
    if (bytes_supported == 2) {
        return val < UINT16_MAX;
    }
    if (bytes_supported == 4) {
        return val < UINT32_MAX;
    }
    return val < UINT64_MAX;
}

view_and_id get_both (const interner_t group_id, const std::string_view s, char bytes_supported)
{
    interner_storage &storage = get_group (group_id.id);

    {  // shared lock scope
        auto sl = std::shared_lock (*storage.mtx);
        auto it = storage.ids_by_string.find (s);
        if (it != storage.ids_by_string.end ())
            return {&it->first, it->second};
        if (!check_valid (storage.strings_by_id.size () + 1, bytes_supported))
            throw std::system_error (ENOMEM,
                                     std::generic_category (),
                                     "Too many strings for configured size");
    }  // release shared lock

    // writer lock scope
    auto ul = std::unique_lock (*storage.mtx);
    const auto &[it, added] = storage.ids_by_string.emplace (s, UINTPTR_MAX);
    if (!added)  // already there
        return {&it->first, it->second};
    it->second = storage.strings_by_id.size ();
    storage.strings_by_id.emplace_back (&it->first);
    return {&it->first, it->second};
}

const std::string *get_by_id (interner_t group_id, uintptr_t string_id)
{
    const interner_storage &storage = get_group (group_id.id);
    return storage.strings_by_id.at (string_id);
}
}  // namespace detail
}  // namespace intern

extern "C" {
/// Get an interned string identifier from group id and a char * and length, the string need not be
/// null terminated
intern_string_t interner_get_str (const interner_t group_id, intern_string_view_t s)
{
    try {
        auto [_, id] = intern::detail::get_both (group_id, {s.str, s.len}, 8);
        return {group_id.id, id};
    } catch (...) {
        // catch all exceptions
        return {group_id, UINTPTR_MAX};
    }
}

/// get the hash of this interned string
size_t intern_str_hash (const intern_string_t s)
{
    return intern::detail::hash_combine (std::hash<std::decay_t<decltype (s.id)> >{}(s.id),
                                         std::hash<decltype (s.group_id.id)>{}(s.group_id.id));
}

/// get a view of the string with both string and length
intern_string_view_t intern_str_view (const intern_string_t s)
{
    try {
        auto str = intern::detail::get_by_id (s.group_id, s.id);
        return {str->data (), str->size ()};
    } catch (...) {
        return {nullptr, 0};
    }
}

/// get the null-terminated string (not recommended, use lengths)
const char *intern_str_cstr (const intern_string_t s)
{
    return intern_str_view (s).str;
}

/// comparator function that orders by ID
int intern_str_cmp_by_id (const void *l, const void *r)
{
    const auto lhs = static_cast<const intern_string_t *> (l);
    const auto rhs = static_cast<const intern_string_t *> (r);
    if (!lhs || !rhs)
        return -1;

    if (lhs->group_id.id != rhs->group_id.id) {
        if (lhs->group_id.id < rhs->group_id.id)
            return -1;
        return 1;
    }
    if (lhs->id != rhs->id) {
        if (lhs->id < rhs->id)
            return -1;
        return 1;
    }
    return 0;
}

/// comparator function that orders by string value
int intern_str_cmp_by_str (const void *l, const void *r)
{
    const auto lhs = static_cast<const intern_string_t *> (l);
    const auto rhs = static_cast<const intern_string_t *> (r);
    if (!lhs || !rhs)
        return -1;

    const auto lstr = intern::detail::get_by_id (lhs->group_id, lhs->id);
    const auto rstr = intern::detail::get_by_id (rhs->group_id, rhs->id);

    return lstr->compare (*rstr);
}
}
