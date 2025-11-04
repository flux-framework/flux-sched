#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c/reapi_cli.h>
#include <fstream>
#include <iostream>

namespace Flux::resource_model::detail {

TEST_CASE ("Initialize REAPI CLI", "[initialize C]")
{
    const std::string options = "{}";
    std::stringstream buffer;
    std::ifstream inputFile ("../../../t/data/resource/grugs/tiny.graphml");

    if (!inputFile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    buffer << inputFile.rdbuf ();
    std::string rgraph = buffer.str ();

    reapi_cli_ctx_t *ctx = nullptr;
    ctx = reapi_cli_new ();
    REQUIRE (ctx);
}

TEST_CASE ("Match basic jobspec", "[match C]")
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

    reapi_cli_ctx_t *ctx = nullptr;
    ctx = reapi_cli_new ();
    REQUIRE (ctx);

    rc = reapi_cli_initialize (ctx, rgraph.c_str (), options.c_str ());
    REQUIRE (rc == 0);

    match_op_t match_op = match_op_t::MATCH_ALLOCATE;
    bool reserved = false;
    char *R;
    uint64_t jobid = 1;
    double ov = 0.0;
    int64_t at = 0;

    rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
    CHECK (rc == 0);
    CHECK (reserved == false);
    CHECK (at == 0);
}

TEST_CASE ("Match basic jobspec without allocating", "[match C]")
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

    reapi_cli_ctx_t *ctx = nullptr;
    ctx = reapi_cli_new ();
    REQUIRE (ctx);

    rc = reapi_cli_initialize (ctx, rgraph.c_str (), options.c_str ());
    REQUIRE (rc == 0);

    bool reserved = false;
    char *R;
    uint64_t jobid = 1;
    double ov = 0.0;
    int64_t at = 0;

    match_op_t match_op = match_op_t::MATCH_WITHOUT_ALLOCATING;

    rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
    CHECK (reserved == false);
    CHECK (at == 0);
    REQUIRE (rc == 0);

    match_op = match_op_t::MATCH_ALLOCATE;

    for (int i = 0; i < 4; i++) {
        rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
        CHECK (reserved == false);
        CHECK (at == 0);
        REQUIRE (rc == 0);
    }
    rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
    REQUIRE (rc == -1);  // The tiny graph should be full

    match_op = match_op_t::MATCH_WITHOUT_ALLOCATING;

    rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
    CHECK (reserved == false);
    CHECK (at == 3600);  // MWOA should match the next available time
    CHECK (rc == 0);
}

}  // namespace Flux::resource_model::detail
