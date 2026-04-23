#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c++/reapi_cli.hpp>
#include <fstream>
#include <cstdlib>

#include "resource/schema/resource_graph.hpp"
#include "resource/policies/base/match_op.h"

namespace Flux::resource_model::detail {

TEST_CASE ("Initialize REAPI CLI", "[initialize C++]")
{
    const std::string options = "{}";
    std::stringstream buffer;
    std::string sharness_test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    std::ifstream inputFile (sharness_test_srcdir + "/data/resource/grugs/tiny.graphml");

    if (!inputFile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    buffer << inputFile.rdbuf ();
    std::string rgraph = buffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);
}

TEST_CASE ("Convert between strings and match_op_ts", "[match C++]")
{
    // string <-> match_op_t idempotence
    // (excluding START_ AND END_ vals, which count as MATCH_UNKNOWN)
    for (int op = START_MATCH_OP_T + 1; op != END_MATCH_OP_T; op++)
        REQUIRE (string_to_match_op (match_op_to_string ((match_op_t)op)) == op);

    // valid match_op_t are valid (assuming the enum order:
    // [start, unknown, valid..., end])
    for (int op = START_MATCH_OP_T + 2; op != END_MATCH_OP_T; op++) {
        REQUIRE (match_op_valid ((match_op_t)op));
    }

    // invalid match_op_t are invalid
    REQUIRE (!match_op_valid (MATCH_UNKNOWN));
    REQUIRE (!match_op_valid (START_MATCH_OP_T));
    REQUIRE (!match_op_valid (END_MATCH_OP_T));
    REQUIRE (!match_op_valid (string_to_match_op ("0xDEADBEEF")));
}

TEST_CASE ("Match basic jobspec", "[match C++]")
{
    int rc = -1;
    const std::string options = "{}";
    std::stringstream gbuffer, jbuffer;
    std::string sharness_test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    std::ifstream graphfile (sharness_test_srcdir + "/data/resource/grugs/tiny.graphml");
    std::ifstream jobspecfile (sharness_test_srcdir
                               + "/data/resource/jobspecs/basics/test006.yaml");

    if (!graphfile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    gbuffer << graphfile.rdbuf ();
    std::string rgraph = gbuffer.str ();

    if (!jobspecfile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    jbuffer << jobspecfile.rdbuf ();
    std::string jobspec = jbuffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);

    match_op_t match_op = match_op_t::MATCH_ALLOCATE;
    bool reserved = false;
    std::string R = "";
    uint64_t jobid = 1;
    double ov = 0.0;
    int64_t at = 0;

    rc = detail::reapi_cli_t::match_allocate (ctx.get (),
                                              match_op,
                                              jobspec,
                                              jobid,
                                              reserved,
                                              R,
                                              at,
                                              ov);
    CHECK (rc == 0);
    CHECK (reserved == false);
    CHECK (at == 0);
}

TEST_CASE ("Match basic jobspec without allocating", "[match C++]")
{
    int rc = -1;
    const std::string options = "{}";
    std::stringstream gbuffer, jbuffer;
    std::string sharness_test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    std::ifstream graphfile (sharness_test_srcdir + "/data/resource/grugs/tiny.graphml");
    std::ifstream jobspecfile (sharness_test_srcdir
                               + "/data/resource/jobspecs/basics/test006.yaml");

    if (!graphfile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    jbuffer << jobspecfile.rdbuf ();
    std::string jobspec = jbuffer.str ();

    if (!jobspecfile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    gbuffer << graphfile.rdbuf ();
    std::string rgraph = gbuffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);

    std::map<vtx_t, pool_infra_t> idata_map;
    std::map<vtx_t, schedule_t> sched_map;
    vtx_iterator_t u, end;
    for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end; u++) {
        idata_map[*u] = ctx->db->resource_graph[*u].idata;
        sched_map[*u] = ctx->db->resource_graph[*u].schedule;
        REQUIRE (idata_map[*u] == ctx->db->resource_graph[*u].idata);
        REQUIRE (sched_map[*u] == ctx->db->resource_graph[*u].schedule);
    }

    bool reserved = false;
    std::string R = "";
    uint64_t jobid = 1;
    double ov = 0.0;
    int64_t at = 0;

    // MWOA should succeed on an empty graph
    match_op_t match_op = match_op_t::MATCH_WITHOUT_ALLOCATING;

    rc = detail::reapi_cli_t::match_allocate (ctx.get (),
                                              match_op,
                                              jobspec,
                                              jobid,
                                              reserved,
                                              R,
                                              at,
                                              ov);
    REQUIRE (rc == 0);
    REQUIRE (reserved == false);
    REQUIRE (at == 0);

    // MWOA_FUTURE should succeed on an empty graph
    match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_FUTURE;

    rc = detail::reapi_cli_t::match_allocate (ctx.get (),
                                              match_op,
                                              jobspec,
                                              jobid,
                                              reserved,
                                              R,
                                              at,
                                              ov);
    REQUIRE (rc == 0);
    REQUIRE (reserved == false);
    REQUIRE (at == 0);

    // Check that the post-MWOA graph state is the same as the initial state
    for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end; u++) {
        CHECK (idata_map.at (*u).has_equal_behavior_to (ctx->db->resource_graph[*u].idata));
        CHECK (sched_map.at (*u) == ctx->db->resource_graph[*u].schedule);
    }

    // Allocate all resources
    match_op = match_op_t::MATCH_ALLOCATE;

    for (int i = 0; i < 4; i++) {
        rc = detail::reapi_cli_t::match_allocate (ctx.get (),
                                                  match_op,
                                                  jobspec,
                                                  jobid,
                                                  reserved,
                                                  R,
                                                  at,
                                                  ov);
        CHECK (reserved == false);
        CHECK (at == 0);
        REQUIRE (rc == 0);
    }

    // The tiny graph should be full
    rc = detail::reapi_cli_t::match_allocate (ctx.get (),
                                              match_op,
                                              jobspec,
                                              jobid,
                                              reserved,
                                              R,
                                              at,
                                              ov);
    REQUIRE (rc == -1);

    // The graph state should have changed
    bool changed = false;
    for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end; u++) {
        changed |= !(idata_map.at (*u).has_equal_behavior_to (ctx->db->resource_graph[*u].idata));
        changed |= !(sched_map.at (*u) == ctx->db->resource_graph[*u].schedule);
    }
    REQUIRE (changed == true);

    // MWOA_FUTURE should match the next available time, which is in the future
    match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_FUTURE;

    rc = detail::reapi_cli_t::match_allocate (ctx.get (),
                                              match_op,
                                              jobspec,
                                              jobid,
                                              reserved,
                                              R,
                                              at,
                                              ov);
    CHECK (reserved == false);
    CHECK (at == 3600);
    CHECK (rc == 0);

    // MWOA should try to match at t=0 and fail because the graph is full
    match_op = match_op_t::MATCH_WITHOUT_ALLOCATING;

    rc = detail::reapi_cli_t::match_allocate (ctx.get (),
                                              match_op,
                                              jobspec,
                                              jobid,
                                              reserved,
                                              R,
                                              at,
                                              ov);
    CHECK (reserved == false);
    CHECK (at == 0);
    CHECK (rc == -1);
}

}  // namespace Flux::resource_model::detail
