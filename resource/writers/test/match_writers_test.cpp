#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <vector>
#include <iostream>

#include "resource/writers/match_writers.hpp"

namespace Flux::resource_model {

TEST_CASE ("Test compress_ids on 0-4", "[match_writers_t]")
{
    std::vector<int64_t> ids = {0, 0, 1, 1, 2, 2, 3, 4};
    std::stringstream o;
    rlite_match_writers_t writer;
    REQUIRE (writer.compress_ids (o, ids) == 0);
    REQUIRE (o.str () == "0-4");
}

TEST_CASE ("Test compress_ids on 1,3,5,7-9", "[match_writers_t]")
{
    std::vector<int64_t> ids = {1, 1, 3, 5, 7, 8, 9};
    std::stringstream o;
    rlite_match_writers_t writer;
    REQUIRE (writer.compress_ids (o, ids) == 0);
    REQUIRE (o.str () == "1,3,5,7-9");
}

TEST_CASE ("Test compress_ids on -1", "[match_writers_t]")
{
    std::vector<int64_t> ids = {-1};
    std::stringstream o;
    rlite_match_writers_t writer;
    REQUIRE (writer.compress_ids (o, ids) == -1);
}

TEST_CASE ("Test compress_ids on -586", "[match_writers_t]")
{
    std::vector<int64_t> ids = {-586};
    std::stringstream o;
    rlite_match_writers_t writer;
    REQUIRE (writer.compress_ids (o, ids) == -1);
}

}  // namespace Flux::resource_model
