#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c++/reapi_cli.hpp>
#include <fstream>

namespace Flux::resource_model::detail {

TEST_CASE ("Initialize REAPI CLI", "[initialize C++]")
{
    const std::string options = "{}";
    std::stringstream buffer;
    std::ifstream inputFile ("../../../t/data/resource/grugs/tiny.graphml");

    if (!inputFile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    buffer << inputFile.rdbuf ();
    std::string rgraph = buffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);
}

TEST_CASE ("Match basic jobspec", "[match C++]")
{
    int rc = -1;
    const std::string options = "{}";
    std::stringstream gbuffer, jbuffer;
    std::ifstream graphfile ("../../../t/data/resource/grugs/tiny.graphml");
    std::ifstream jobspecfile ("../../../t/data/resource/jobspecs/basics/test006.yaml");

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
    std::ifstream graphfile ("../../../t/data/resource/grugs/tiny.graphml");
    std::ifstream jobspecfile ("../../../t/data/resource/jobspecs/basics/test006.yaml");

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
