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
#include <ostream>

#include <boost/container/small_vector.hpp>
#include <boost/optional/optional.hpp>
#include <stdexcept>

namespace intern {
namespace detail {
struct view_and_id {
    const std::string *view;
    uint64_t id;
};

view_and_id get_both (interner_t group_id, std::string_view s, char bytes_supported);

const std::string *get_by_id (interner_t group_id, uintptr_t string_id);
};  // namespace detail

/// A convenience wrapper of an interned string
template<uint64_t Tag, class Id>
    requires (sizeof (Id) <= sizeof (uintptr_t))
class interned_string {
    Id _id = 0;

    // Not at all safe, verified by a pre-condition check in get_by_id
    explicit interned_string (Id id) : _id (id)
    {
    }

   public:
    using id_type = Id;
    static constexpr interner_t interner{Tag};
    static constexpr Id invalid_val = std::numeric_limits<Id>::max;

    explicit interned_string (const std::string_view sv)
        : _id (detail::get_both (interner, sv, sizeof (Id)).id)
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
        detail::get_by_id (Tag, id);
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
        return *detail::get_by_id (interner, _id);
    }

    const char *c_str () const
    {
        return get ().c_str ();
    }

    Id id () const
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
template<uint64_t Id, typename U>
struct hash<intern::interned_string<Id, U> > {
    size_t operator() (const intern::interned_string<Id, U> &s) const
    {
        return intern_str_hash (intern_string_t{s.interner, s.id ()});
    }
};
}  // namespace std
#endif  // INTERNER_LIBRARY_H
