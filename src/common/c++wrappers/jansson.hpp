//
// Created by Scogland, Tom on 8/11/24.
//

#ifndef JANSONN_HPP
#define JANSONN_HPP
#include <boost/icl/detail/map_algo.hpp>

extern "C" {
#include <jansson.h>
}
namespace json {
struct no_incref {};
class value {
    json_t *_v = nullptr;

   public:
    value () = default;
    value (value &&rhs)
    {
        json_decref (_v);
        _v = rhs._v;
        rhs._v = nullptr;
    }
    value &operator= (value &&rhs)
    {
        json_decref (_v);
        _v = rhs._v;
        rhs._v = nullptr;
        return *this;
    }
    value (value const &rhs)
    {
        json_decref (_v);
        _v = rhs._v;
        json_incref (_v);
    }
    value &operator= (value const &rhs)
    {
        json_decref (_v);
        _v = rhs._v;
        json_incref (_v);
        return *this;
    }
    value (json_t *v) : _v (v)
    {
        json_incref (_v);
    }
    value (no_incref, json_t *v) : _v (v)
    {
    }
    ~value ()
    {
        json_decref (_v);
    }
    static value take (json_t *v)
    {
        return value (no_incref{}, v);
    }

    json_t *get ()
    {
        return _v;
    }

    const json_t *get () const
    {
        return _v;
    }

    explicit operator json_t * ()
    {
        return _v;
    }
    operator bool ()
    {
        return _v;
    }

    value &emplace_object ()
    {
        json_decref (_v);
        _v = json_object ();
        return *this;
    }
    value &emplace_array ()
    {
        json_decref (_v);
        _v = json_array ();
        return *this;
    }
};
template<typename T>
concept Valuable = requires (T const &v) {
    { ::json::value (v) };
};
template<typename T>
    requires Valuable<T>
inline void to_json (value &jv, T const &v)
{
    jv = value (v);
}
inline void to_json (value &jv, std::string_view const s)
{
    jv = value (no_incref{}, json_stringn (s.data (), s.length ()));
}
template<typename T>
concept Maplike = requires (T const &v) {
    {
        std::is_same_v<typename T::mapped_type,
                       std::pair<typename T::key_type, typename T::value_type>>
    };
};
template<typename MapT>
    requires Maplike<MapT>
inline void to_json (value &jv, const MapT &m)
{
    jv.emplace_object ();
    for (auto &[k, v] : m) {
        value val;
        to_json (val, v);
        json_object_set (jv.get (), k.c_str (), val.get ());
    }
}
}  // namespace json

#endif  // JANSONN_HPP
