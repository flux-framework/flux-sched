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
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace intern {
namespace detail {
std::shared_mutex group_mtx;

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

struct dense_inner_storage {
    std::vector<const std::string *> strings_by_id;
    // Tombstone is UINTPTR_MAX
    std::unordered_map<std::string, size_t, string_hash, std::equal_to<>> ids_by_string;
    // reader/writer lock to protect entries
    std::unique_ptr<std::shared_mutex> mtx = std::make_unique<std::shared_mutex> ();
    // if the object is finalized and not opened, reject new strings
    bool finalized = false;
    // whether this object is allowed to accept new strings after finalization from each thread
    std::unordered_map<std::thread::id, bool> open_map;
};

dense_inner_storage &get_dense_inner_storage (size_t unique_id)
{
    static std::unordered_map<size_t, dense_inner_storage> groups;
    auto guard = std::unique_lock (group_mtx);
    return groups[unique_id];
}

void dense_storage_finalize (dense_inner_storage &storage)
{
    auto ul = std::unique_lock (*storage.mtx);
    storage.finalized = true;
}
void dense_storage_open (dense_inner_storage &storage)
{
    auto ul = std::unique_lock (*storage.mtx);
    storage.open_map[std::this_thread::get_id ()] = true;
}
void dense_storage_close (dense_inner_storage &storage)
{
    auto ul = std::unique_lock (*storage.mtx);
    storage.open_map[std::this_thread::get_id ()] = false;
}

struct sparse_inner_storage;
struct sparse_string_node {
    // This must be carefully managed, it holds onto the sparse_inner_storage that is keeping the
    // string alive until all strings in that table have been reclaimed. It looks cyclic, but
    // because the strings are stored by weak_ptr, and remove these entries when they expire, it is
    // not.
    std::shared_ptr<sparse_inner_storage> storage;
    typename rc_str_t::weak_type str;
};
struct sparse_inner_storage : std::enable_shared_from_this<sparse_inner_storage> {
    // Tombstone is UINTPTR_MAX
    std::unordered_map<std::string, sparse_string_node, string_hash, std::equal_to<>> strings;
    // reader/writer lock to protect entries
    std::unique_ptr<std::shared_mutex> mtx = std::make_unique<std::shared_mutex> ();
};

sparse_inner_storage *get_sparse_inner_storage (size_t unique_id)
{
    static std::unordered_map<size_t, std::shared_ptr<sparse_inner_storage>> groups;
    auto guard = std::unique_lock (group_mtx);
    auto &ret = groups[unique_id];
    if (!ret)
        ret = std::make_shared<sparse_inner_storage> ();
    return ret.get ();
}

void remove_rc (sparse_inner_storage *storage, const std::string *s)
{
    // The lifetime of storage must exceed the life of the lock region below or we may access
    // invalid memory on the unlock phase because of destructing the storage after the last string
    auto ref = storage->shared_from_this ();
    {
        // writer lock scope
        auto ul = std::unique_lock (*storage->mtx);
        storage->strings.erase (*s);
    }
}

rc_str_t get_rc (sparse_inner_storage *storage,
                 std::string_view s,
                 rc_free_fn fn,
                 bool add_if_missing)
{
    {  // shared lock scope
        auto sl = std::shared_lock (*storage->mtx);
        auto it = storage->strings.find (s);
        if (it != storage->strings.end ()) {
            if (auto shared = it->second.str.lock ())
                return shared;
            // we found it, but the weak pointer is no longer valid somehow, this shouldn't be
            // possible but handle it anyway
        }
    }  // release shared lock

    if (!add_if_missing)
        return nullptr;

    // writer lock scope
    auto ul = std::unique_lock (*storage->mtx);
    // create the shared pointer to the sparse_string_node, and ensure
    auto ssn = sparse_string_node{storage->shared_from_this (), rc_str_t (nullptr)};
    const auto &[it, added] = storage->strings.emplace (s, ssn);
    // create shared pointer referring to the key, which will be stored as a weak pointer in the
    // value, using the passed deleter which will remove it from the map when no user references
    // remain
    auto shared = rc_str_t (&it->first, fn);

    it->second.str = shared;

    return shared;
}

static bool check_valid (size_t val, char bytes_supported)
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

view_and_id get_both (dense_inner_storage &storage, const std::string_view s, char bytes_supported)
{
    {  // shared lock scope
        auto sl = std::shared_lock (*storage.mtx);
        auto it = storage.ids_by_string.find (s);
        if (it != storage.ids_by_string.end ())
            return {&it->first, it->second};
        if (!check_valid (storage.strings_by_id.size () + 1, bytes_supported))
            throw std::system_error (ENOMEM,
                                     std::generic_category (),
                                     "Too many strings for configured size");
        // if storage is finalized and the thread isn't in the open map, we must not add another
        // here, check while under the shared lock
        if (storage.finalized && !storage.open_map[std::this_thread::get_id ()]) {
            using namespace std::string_literals;
            std::string err =
                "This interner is finalized and must be open to add strings, found new string: '"s
                + std::string (s) + "'"s;
            throw std::system_error (ENOMEM, std::generic_category (), err);
        }
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

const std::string *get_by_id (dense_inner_storage &storage, size_t string_id)
{
    auto sl = std::shared_lock (*storage.mtx);
    return storage.strings_by_id.at (string_id);
}

std::size_t hash_combine (std::size_t lhs, std::size_t rhs)
{
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}
}  // namespace detail
}  // namespace intern

extern "C" {
/// Get an interned string identifier from group id and a char * and length, the string need not be
/// null terminated
intern_string_t dense_interner_get_str (const interner_t group_id, intern_string_view_t s)
{
    try {
        auto [_, id] =
            intern::detail::get_both (intern::detail::get_dense_inner_storage (group_id.id),
                                      {s.str, s.len},
                                      8);
        return {group_id.id, id};
    } catch (...) {
        // catch all exceptions
        return {group_id, UINTPTR_MAX};
    }
}

/// get the hash of this interned string
size_t intern_str_hash (const intern_string_t s)
{
    return intern::detail::hash_combine (std::hash<std::decay_t<decltype (s.id)>>{}(s.id),
                                         std::hash<decltype (s.group_id.id)>{}(s.group_id.id));
}

/// get a view of the string with both string and length
intern_string_view_t intern_str_view (const intern_string_t s)
{
    try {
        auto str =
            intern::detail::get_by_id (intern::detail::get_dense_inner_storage (s.group_id.id),
                                       s.id);
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

    auto &ls = intern::detail::get_dense_inner_storage (lhs->group_id.id);
    auto &rs = intern::detail::get_dense_inner_storage (rhs->group_id.id);
    const auto lstr = intern::detail::get_by_id (ls, lhs->id);
    const auto rstr = intern::detail::get_by_id (rs, rhs->id);

    return lstr->compare (*rstr);
}
}
