/*****************************************************************************\
* Copyright 2024 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef INTERNER_LIBRARY_H
#define INTERNER_LIBRARY_H

#include "interner.h"

#include <stdint.h>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <math.h>

#include "scope_guard.hpp"
#include <boost/container/small_vector.hpp>
#include <boost/optional/optional.hpp>
#include <stdexcept>

namespace intern {
namespace detail {
struct view_and_id {
    const std::string *view;
    size_t id;
};

// forward declarations
struct dense_inner_storage;
view_and_id get_both (dense_inner_storage &ds, std::string_view s, char bytes_supported);
const std::string *get_by_id (dense_inner_storage &ds, size_t string_id);

void dense_storage_finalize (dense_inner_storage &storage);
void dense_storage_open (dense_inner_storage &storage);
void dense_storage_close (dense_inner_storage &storage);

dense_inner_storage &get_dense_inner_storage (size_t unique_id);

std::size_t hash_combine (std::size_t lhs, std::size_t rhs);
};  // namespace detail

/// interner storage class providing dense, in-order IDs of configurable size
///
/// allows addition of strings until finalized, then after that only when explicitly opened for
/// additions, this is not for thread safety, it is to prevent denial of service from addition of
/// invalid types through interfaces that take user input
template<class Tag, class Id>
    requires (sizeof (Id) <= sizeof (size_t))
struct dense_storage {
    // forward declare for reference and static declaration
    using tag_t = Tag;
    using id_instance_t = Id;
    using id_storage_t = size_t;

    dense_storage (dense_storage const &) = delete;
    dense_storage (dense_storage &&) = delete;
    dense_storage &operator= (dense_storage const &) = delete;
    dense_storage &operator= (dense_storage &&) = delete;

   private:
    static detail::dense_inner_storage &get_storage ()
    {
        // C++ guarantees atomic initialization of a static like this, template expansion makes
        // __PRETTY_FUNCTION__ unique for valid programs, using function here to avoid having to
        // include heavy headers in this header
        static detail::dense_inner_storage &s =
            ::intern::detail::get_dense_inner_storage (dense_storage::unique_id ());
        return s;
    };

   public:
    static detail::view_and_id get_both (std::string_view s);
    static id_instance_t get_id (std::string_view s);
    static const std::string *get_by_id (id_storage_t string_id);

    static size_t unique_id ()
    {
        static size_t uid = std::hash<std::string_view> () (__PRETTY_FUNCTION__);
        return uid;
    }

    /// stop new strings from being added unless the object is explicitly opened
    static void finalize ()
    {
        detail::dense_storage_finalize (get_storage ());
    }
    /// open the interner for additions and auto-close on scope exit
    [[nodiscard]] static auto open_for_scope ()
    {
        open ();
        return sg::make_scope_guard ([] () { close (); });
    }

    /// open the interner for additions
    static auto open ()
    {
        detail::dense_storage_open (get_storage ());
    }
    /// close the interner for additions
    static void close ()
    {
        detail::dense_storage_close (get_storage ());
    }
};

template<class Tag, class Id>
    requires (sizeof (Id) <= sizeof (unsigned long int))
detail::view_and_id dense_storage<Tag, Id>::get_both (std::string_view s)
{
    return ::intern::detail::get_both (dense_storage::get_storage (), s, sizeof (Id));
}
template<class Tag, class Id>
    requires (sizeof (Id) <= sizeof (unsigned long int))
Id dense_storage<Tag, Id>::get_id (std::string_view s)
{
    return ::intern::detail::get_both (dense_storage::get_storage (), s, sizeof (Id)).id;
}
template<class Tag, class Id>
    requires (sizeof (Id) <= sizeof (unsigned long int))
const std::string *dense_storage<Tag, Id>::get_by_id (id_storage_t string_id)
{
    return ::intern::detail::get_by_id (dense_storage::get_storage (), string_id);
}

namespace detail {
struct sparse_inner_storage;
sparse_inner_storage *get_sparse_inner_storage (size_t);
using rc_str_t = std::shared_ptr<const std::string>;
typedef void (*rc_free_fn) (const std::string *s);
rc_str_t get_rc (sparse_inner_storage *storage,
                 std::string_view s,
                 rc_free_fn fn,
                 bool add_if_missing = true);
void remove_rc (sparse_inner_storage *is, const std::string *s);
}  // namespace detail
/// interner storage class providing refcounted interned strings, for untrusted inputs and large
/// sets
template<class Tag>
struct rc_storage {
    // forward declare for reference and static declaration
    using tag_t = Tag;
    using id_instance_t = detail::rc_str_t;
    using id_storage_t = detail::rc_str_t;

    rc_storage (rc_storage const &) = delete;
    rc_storage (rc_storage &&) = delete;
    rc_storage &operator= (rc_storage const &) = delete;
    rc_storage &operator= (rc_storage &&) = delete;

    static id_instance_t get_id (std::string_view s)
    {
        return ::intern::detail::get_rc (rc_storage::get_storage (), s, deleter);
    }
    static const std::string *get_by_id (id_storage_t string_id)
    {
        return string_id.get ();
    }
    static size_t unique_id ()
    {
        return std::hash<std::string_view> () (__PRETTY_FUNCTION__);
    }
    static bool contains (std::string_view s)
    {
        return ::intern::detail::get_rc (rc_storage::get_storage (), s, deleter, false) != nullptr;
    }

   private:
    static void deleter (const std::string *s)
    {
        remove_rc (rc_storage::get_storage (), s);
    }
    static detail::sparse_inner_storage *get_storage ()
    {
        // template expansion makes __PRETTY_FUNCTION__ unique for valid programs, using function
        // here to avoid having to include heavy headers in this header
        // This is stored as a pointer because the pointer will not be destructed at the end
        // of execution, so we can refer to it even if the shared_pointer in
        // get_sparse_inner_storage has been released. This is only safe because we can only get
        // here if a shared_pointer that holds onto a reference to this still exists somewhere.
        static auto *ptr_to_storage =
            ::intern::detail::get_sparse_inner_storage (rc_storage::unique_id ());
        return ptr_to_storage;
    }
};

/// A convenience wrapper of an interned string
template<class Storage>
class interned_string {
    using Id = typename Storage::id_instance_t;
    Id _id;

    // Not at all safe, verified by a pre-condition check in get_by_id
    explicit interned_string (Id id) : _id (id)
    {
    }

   public:
    using storage_t = Storage;
    using id_type = Id;

    explicit interned_string (const std::string_view sv) : _id (Storage::get_id (sv))
    {
    }

    interned_string () : interned_string ("")
    {
    }

    interned_string (const interned_string &o) = default;

    interned_string (interned_string &&) = default;

    interned_string &operator= (const interned_string &) = default;

    interned_string &operator= (interned_string &&) = default;

    /// interned strings are ordered by insertion not value
    auto operator<=> (const interned_string &) const = default;

    /// identity comparison
    bool operator== (const interned_string &) const = default;

    static interned_string make (std::string_view v)
    {
        return interned_string (v);
    }

    static interned_string get_by_id (Id id)
    {
        // will throw if this does not exist
        Storage::get_by_id (id);
        return interned_string (id);
    }

    struct istring_end {
        Id _val;
    };

    class istring_incrementable {
        Id _cur;

       public:
        explicit istring_incrementable (interned_string cur) : _cur{cur.id ()}
        {
        }

        explicit istring_incrementable (Id cur_id) : _cur{cur_id}
        {
        }

        istring_incrementable (const istring_incrementable &o) = default;

        istring_incrementable (istring_incrementable &&) = default;

        istring_incrementable &operator= (const istring_incrementable &) = default;

        istring_incrementable &operator= (istring_incrementable &&) = default;

        istring_incrementable &operator++ ()
        {
            ++_cur;
            return *this;
        }
        bool operator== (istring_end o)
        {
            return o._val == _cur;
        }
        interned_string operator* ()
        {
            return interned_string{_cur};
        }
    };

    class istring_range {
        Id _first;
        Id _one_past_last;

       public:
        istring_range (const istring_range &o) = default;

        istring_range (istring_range &&) = default;

        istring_range &operator= (const istring_range &) = default;

        istring_range &operator= (istring_range &&) = default;

        istring_range (Id first, Id one_past_last) : _first{first}, _one_past_last{one_past_last}
        {
        }

        istring_incrementable begin ()
        {
            if (_first >= _one_past_last)
                return istring_incrementable{_one_past_last};
            return istring_incrementable{_first};
        }
        istring_end end ()
        {
            return {_one_past_last};
        }
    };

    static istring_range range_inclusive (interned_string first, interned_string last)
    {
        return {first._id, last._id + 1};
    }

    static istring_range range_by_ids (Id first, Id one_past_last)
    {
        return {first, one_past_last};
    }

    const std::string &get () const
    {
        return *Storage::get_by_id (_id);
    }

    const char *c_str () const
    {
        return get ().c_str ();
    }

    Id const &id () const
    {
        return _id;
    }

    friend std::string operator+ (std::string const &lhs, interned_string const &rhs)
    {
        return lhs + rhs.get ();
    }
    friend std::string operator+ (interned_string const &lhs, std::string const &rhs)
    {
        return lhs.get () + rhs;
    }
    friend std::ostream &operator<< (std::ostream &os, const interned_string &obj)
    {
        return os << obj.get ();
    }
    friend std::istream &operator>> (std::istream &os, interned_string &obj)
    {
        std::string tmp;
        os >> tmp;
        obj = interned_string{tmp};
        return os;
    }
};
template<class InternT, class T, int likely_count = 2>
struct interned_key_vec : boost::container::small_vector<T, likely_count> {
    using base = boost::container::small_vector<T, likely_count>;
    T &operator[] (InternT s)
    {
        if (s.id () >= base::size ()) [[unlikely]]
            this->resize (s.id () + 1);
        return base::operator[] (s.id ());
    }

    T &at (InternT s)
    {
        if (s.id () >= base::size ()) [[unlikely]]
            throw std::out_of_range ("no element available");
        return base::operator[] (s.id ());
    }

    const T &at (InternT s) const
    {
        if (s.id () >= base::size ()) [[unlikely]]
            throw std::out_of_range ("no element available");
        return base::operator[] (s.id ());
    }

    boost::optional<T &> try_at (InternT s)
    {
        if (s.id () >= base::size ()) [[unlikely]]
            return boost::none;
        return base::operator[] (s.id ());
    }

    boost::optional<const T &> try_at (InternT s) const
    {
        if (s.id () >= base::size ()) [[unlikely]]
            return boost::none;
        return base::operator[] (s.id ());
    }

    typename InternT::istring_range key_range () const
    {
        return InternT::range_by_ids (0, base::size ());
    }
};
}  // namespace intern

namespace std {
namespace detail {
template<typename Tval>
struct FastPointerHash {
    size_t operator() (const Tval *val) const
    {
        static const size_t shift = (size_t)log2 (1 + sizeof (Tval));
        return (size_t)(val) >> shift;
    }
};
};  // namespace detail
template<class Tag>
struct hash<intern::interned_string<intern::rc_storage<Tag>>> {
    using Storage = intern::rc_storage<Tag>;
    using T = intern::interned_string<Storage>;
    size_t operator() (const T &s) const
    {
        return ::intern::detail::
            hash_combine (Storage::unique_id (),
                          detail::FastPointerHash<
                              typename Storage::id_instance_t::element_type> () (s.id ().get ()));
    }
};
template<class Storage>
struct hash<intern::interned_string<Storage>> {
    size_t operator() (const intern::interned_string<Storage> &s) const
    {
        return ::intern::detail::hash_combine (Storage::unique_id (),
                                               std::hash<typename Storage::id_instance_t> () (
                                                   s.id ()));
    }
};
}  // namespace std
#endif  // INTERNER_LIBRARY_H
