#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c++/reapi_cli.hpp>
#include <resource/policies/base/match_op.h>
#include <resource/schema/resource_graph.hpp>
#include <fstream>
#include <cstdlib>

namespace Flux::resource_model::detail {

TEST_CASE ("Initialize REAPI CLI", "[initialize C++]")
{
    const std::string options = "{}";
    std::stringstream buffer;
    const char *test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    REQUIRE (test_srcdir);

    std::ifstream inputFile (std::string (test_srcdir) + "/data/resource/grugs/tiny.graphml");
    REQUIRE (inputFile.is_open ());

    buffer << inputFile.rdbuf ();
    std::string rgraph = buffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);
}

TEST_CASE ("Convert between strings and match_op_ts", "[match C++]")
{
    errno = 0;

    // MATCH_UNKNOWN is invalid and converts to nullptr + ENOENT
    CHECK (!match_op_valid (MATCH_UNKNOWN));
    CHECK (errno == 0);

    CHECK (match_op_to_string (MATCH_UNKNOWN) == nullptr);
    CHECK (errno == ENOENT);
    errno = 0;

    // invalid strings convert to MATCH_UNKNOWN + ENOENT
    CHECK (match_op_from_string ("0xDEADBEEF") == MATCH_UNKNOWN);
    CHECK (errno == ENOENT);
    errno = 0;

    // out-of-bounds enum values are invalid and convert to nullptr + EINVAL
    for (match_op_t OOB_match_op :
         {(match_op_t)(-1), END_MATCH_OP_T, (match_op_t)(END_MATCH_OP_T + 1)}) {
        CHECK (!match_op_valid (OOB_match_op));
        CHECK (errno == 0);

        CHECK (match_op_to_string (OOB_match_op) == nullptr);
        CHECK (errno == EINVAL);
        errno = 0;
    }

    // nullptr converts to MATCH_UNKNOWN + EINVAL (since invalid ptr)
    CHECK (match_op_from_string (nullptr) == MATCH_UNKNOWN);
    CHECK (errno == EINVAL);
    errno = 0;

    // valid match_op_t are valid
    // and match_op_t <-> string conversion is idempotent
    for (int op = MATCH_UNKNOWN + 1; op != END_MATCH_OP_T; op++) {
        CHECK (match_op_valid ((match_op_t)op));
        CHECK (match_op_from_string (match_op_to_string ((match_op_t)op)) == op);
    }
    CHECK (errno == 0);

    // each match_op_t has the correct string representation
    CHECK (match_op_from_string ("allocate") == MATCH_ALLOCATE);
    CHECK (match_op_from_string ("allocate_orelse_reserve") == MATCH_ALLOCATE_ORELSE_RESERVE);
    CHECK (match_op_from_string ("allocate_with_satisfiability")
           == MATCH_ALLOCATE_W_SATISFIABILITY);
    CHECK (match_op_from_string ("satisfiability") == MATCH_SATISFIABILITY);
    CHECK (match_op_from_string ("without_allocating") == MATCH_WITHOUT_ALLOCATING);
    CHECK (match_op_from_string ("without_allocating_future") == MATCH_WITHOUT_ALLOCATING_FUTURE);
    CHECK (errno == 0);
}

TEST_CASE ("Match basic jobspec", "[match C++]")
{
    int rc = -1;
    const std::string options = "{}";
    std::stringstream gbuffer, jbuffer;
    const char *test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    REQUIRE (test_srcdir);

    std::ifstream graphfile (std::string (test_srcdir) + "/data/resource/grugs/tiny.graphml");
    REQUIRE (graphfile.is_open ());

    gbuffer << graphfile.rdbuf ();
    std::string rgraph = gbuffer.str ();

    std::ifstream jobspecfile (std::string (test_srcdir)
                               + "/data/resource/jobspecs/basics/test006.yaml");
    REQUIRE (jobspecfile.is_open ());

    jbuffer << jobspecfile.rdbuf ();
    std::string jobspec = jbuffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);

    SECTION ("MATCH_ALLOCATE")
    {
        match_op_t match_op = MATCH_ALLOCATE;
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

    SECTION ("MATCH_WITHOUT_ALLOCATING[_FUTURE]")
    {
        std::map<vtx_t, pool_infra_t> idata_map;
        std::map<vtx_t, schedule_t> sched_map;
        vtx_iterator_t u, end;
        for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end;
             u++) {
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
        match_op_t match_op = MATCH_WITHOUT_ALLOCATING;
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
        match_op = MATCH_WITHOUT_ALLOCATING_FUTURE;
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
        for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end;
             u++) {
            CHECK (idata_map.at (*u).has_equal_behavior_to (ctx->db->resource_graph[*u].idata));
            CHECK (sched_map.at (*u) == ctx->db->resource_graph[*u].schedule);
        }

        // Allocate all resources
        match_op = MATCH_ALLOCATE;
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
        for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end;
             u++) {
            changed |=
                !(idata_map.at (*u).has_equal_behavior_to (ctx->db->resource_graph[*u].idata));
            changed |= !(sched_map.at (*u) == ctx->db->resource_graph[*u].schedule);
        }
        REQUIRE (changed == true);

        // MWOA_FUTURE should match the next available time, which is in the future
        match_op = MATCH_WITHOUT_ALLOCATING_FUTURE;
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
        match_op = MATCH_WITHOUT_ALLOCATING;
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
}

TEST_CASE ("Initialize REAPI CLI and test add and remove", "[initialize-add-remove C++]")
{
    const std::string options = "{\"load_format\": \"jgf\"}";
    std::stringstream base_buf, add_buf;
    const char *test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    REQUIRE (test_srcdir);
    const std::string data = std::string (test_srcdir) + "/data/resource/jgfs/elastic/";

    std::ifstream base_file (data + "node-test.json");
    REQUIRE (base_file.is_open ());
    base_buf << base_file.rdbuf ();
    const std::string rgraph = base_buf.str ();

    std::ifstream add_file (data + "node-add-test.json");
    REQUIRE (add_file.is_open ());
    add_buf << add_file.rdbuf ();
    const std::string add_sgraph = add_buf.str ();

    const std::string remove_path = "/medium0/rack0/node0";

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);

    // Empty inputs must fail.
    REQUIRE (detail::reapi_cli_t::add_subgraph (ctx.get (), "") != 0);
    REQUIRE (detail::reapi_cli_t::remove_subgraph (ctx.get (), "") != 0);

    // Valid add then remove.
    CHECK (detail::reapi_cli_t::add_subgraph (ctx.get (), add_sgraph) == 0);
    CHECK (detail::reapi_cli_t::remove_subgraph (ctx.get (), remove_path) == 0);
}

TEST_CASE ("Test the graph idempotence of certain match operations", "[match C++]")
{
    int rc = -1;
    const std::string options = "{}";
    std::stringstream gbuffer, jbuffer;
    const char *test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    REQUIRE (test_srcdir);

    std::ifstream graphfile (std::string (test_srcdir) + "/data/resource/grugs/tiny.graphml");
    REQUIRE (graphfile.is_open ());

    gbuffer << graphfile.rdbuf ();
    std::string rgraph = gbuffer.str ();

    std::ifstream jobspecfile (std::string (test_srcdir)
                               + "/data/resource/jobspecs/basics/test006.yaml");
    REQUIRE (jobspecfile.is_open ());

    jbuffer << jobspecfile.rdbuf ();
    std::string jobspec = jbuffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);

    // Store the initial state of the graph
    std::map<vtx_t, pool_infra_t> idata_map;
    std::map<vtx_t, schedule_t> sched_map;
    vtx_iterator_t u, end;
    for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end; u++) {
        idata_map[*u] = ctx->db->resource_graph[*u].idata;
        sched_map[*u] = ctx->db->resource_graph[*u].schedule;
        REQUIRE (idata_map[*u] == ctx->db->resource_graph[*u].idata);
        REQUIRE (sched_map[*u] == ctx->db->resource_graph[*u].schedule);
    }

    match_op_t match_op;
    bool reserved = false;
    std::string R = "";
    uint64_t jobid = 1;
    double ov = 0.0;
    int64_t at = 0;

    SECTION ("MATCH_SATISFIABILITY doesn't change the graph")
    {
        match_op = MATCH_SATISFIABILITY;
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

        // Check that the post-MATCH_SATISFIABILITY graph state is the same as the initial state
        for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end;
             u++) {
            CHECK (idata_map.at (*u).has_equal_behavior_to (ctx->db->resource_graph[*u].idata));
            CHECK (sched_map.at (*u) == ctx->db->resource_graph[*u].schedule);
        }
    }

    SECTION ("MATCH_ALLOCATE changes the graph")
    {
        match_op = MATCH_ALLOCATE;
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

        // The graph state should have changed
        bool changed = false;
        for (boost::tuples::tie (u, end) = boost::vertices (ctx->db->resource_graph); u != end;
             u++) {
            changed |=
                !(idata_map.at (*u).has_equal_behavior_to (ctx->db->resource_graph[*u].idata));
            changed |= !(sched_map.at (*u) == ctx->db->resource_graph[*u].schedule);
        }
        REQUIRE (changed == true);
    }
}

}  // namespace Flux::resource_model::detail
