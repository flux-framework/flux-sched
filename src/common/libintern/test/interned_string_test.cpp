//
// Created by Scogland, Tom on 4/13/23.
//

#include <iostream>
#include <catch2/catch_test_macros.hpp>
#include <interner.hpp>

using namespace intern;
using namespace std::string_view_literals;

struct tag1 {};
struct tag2 {};
using tt1 = interned_string<dense_storage<tag1, uint8_t>>;
using tt2 = interned_string<dense_storage<tag2, uint32_t>>;

TEST_CASE ("interner: intern a single literal, get back the same", "[interner]")
{
    auto l = tt1 ("test"sv);
    auto r = tt1 ("test"sv);
    auto s = tt1 ("test"sv);
    auto s2 = tt1 ("test2"sv);
    REQUIRE (l.get () == r.get ());
    REQUIRE (l.get ().data () == r.get ().data ());
    REQUIRE (l.id () == r.id ());
    REQUIRE (l == r);
    REQUIRE (l.get () == s.get ());
    REQUIRE (l.get ().data () == s.get ().data ());
    REQUIRE (l.id () == s.id ());
    REQUIRE (l == s);
    REQUIRE (l.get () != s2.get ());
    REQUIRE (l.id () != s2.id ());
    REQUIRE (l != s2);
}

TEST_CASE ("interner: strings with different tags compare different", "[interner]")
{
    auto l = tt1 ("test"sv);
    auto r = tt2 ("test"sv);
    REQUIRE (l.get () == r.get ());
    REQUIRE (l.id () == r.id ());
    REQUIRE (l.get ().data () != r.get ().data ());
}

TEST_CASE ("interner: smaller ID limits properly", "[interner]")
{
    struct new_tag {};
    using tt = intern::interned_string<dense_storage<new_tag, uint8_t>>;
    for (int i = 0; i < 254; ++i) {
        std::string s = std::to_string (i);
        tt::make (s);
    }
    auto otm = "one too many"sv;
    CHECK_THROWS (tt::make (otm));
    REQUIRE_NOTHROW (
        dense_interner_get_str (interner_t{tt::storage_t::unique_id ()}, {otm.data (), otm.size ()})
            .id
        == UINTPTR_MAX);
}
struct rctag1 {};
struct rctag2 {};
using trc1 = interned_string<rc_storage<rctag1>>;
using trc2 = interned_string<rc_storage<rctag2>>;
TEST_CASE ("interner rc: intern a single literal, get back the same", "[interner]")
{
    auto l = trc1 ("test"sv);
    auto r = trc1 ("test"sv);
    auto s = trc1 ("test"sv);
    auto s2 = trc1 ("test2"sv);
    REQUIRE (l.get () == r.get ());
    REQUIRE (l.get ().data () == r.get ().data ());
    REQUIRE (l.id () == r.id ());
    REQUIRE (l == r);
    REQUIRE (l.get () == s.get ());
    REQUIRE (l.get ().data () == s.get ().data ());
    REQUIRE (l.id () == s.id ());
    REQUIRE (l == s);
    REQUIRE (l.get () != s2.get ());
    REQUIRE (l.id () != s2.id ());
    REQUIRE (l != s2);
}
TEST_CASE ("interner rc: strings with different tags compare different", "[interner]")
{
    auto l = trc1 ("test"sv);
    auto r = trc2 ("test"sv);
    REQUIRE (l.get () == r.get ());
    // unlike dense, rc will likely have different IDs between two interners
    // REQUIRE (l.id () == r.id ());
    REQUIRE (l.get ().data () != r.get ().data ());
}

TEST_CASE ("interner rc: strings get removed when unused", "[interner]")
{
    REQUIRE_FALSE (trc1::storage_t::contains ("test"sv));
    REQUIRE_FALSE (trc1::storage_t::contains ("test2"sv));
    {
        auto l = trc1 ("test"sv);
        REQUIRE (trc1::storage_t::contains ("test"sv));
    }
    REQUIRE_FALSE (trc1::storage_t::contains ("test"sv));
}
