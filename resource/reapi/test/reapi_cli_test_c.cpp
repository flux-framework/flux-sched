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
    std::ifstream inputFile ("../../../t/data/resource/jgfs/tiny.json");

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
    std::ifstream graphfile ("../../../t/data/resource/jgfs/tiny.json");
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
    REQUIRE (rc == 0);
    REQUIRE (reserved == false);
    REQUIRE (at == 0);
}

TEST_CASE ("Match shrink basic jobspec", "[match shrink C]")
{
    int rc = -1;
    const std::string options = "{\"load_format\": \"rv1exec\"}";
    std::stringstream gbuffer, jbuffer, cbuffer;
    std::ifstream graphfile ("../../../t/data/resource/rv1exec/tiny_rv1exec.json");
    std::ifstream jobspecfile ("../../../t/data/resource/jobspecs/cancel/test018.yaml");
    std::ifstream cancelfile ("../../../t/data/resource/rv1exec/cancel/rank1_cancel.json");

    if (!graphfile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    if (!jobspecfile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    if (!cancelfile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    gbuffer << graphfile.rdbuf ();
    std::string rgraph = gbuffer.str ();
    jbuffer << jobspecfile.rdbuf ();
    std::string jobspec = jbuffer.str ();
    cbuffer << cancelfile.rdbuf ();
    std::string cancel_string = cbuffer.str ();

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
    REQUIRE (rc == 0);
    REQUIRE (reserved == false);
    REQUIRE (at == 0);

    bool noent_ok = false;
    bool full_removal = true;
    rc = reapi_cli_partial_cancel (ctx, jobid, cancel_string.c_str (), noent_ok, &full_removal);
    REQUIRE (rc == 0);
    REQUIRE (full_removal == false);
}

}  // namespace Flux::resource_model::detail
