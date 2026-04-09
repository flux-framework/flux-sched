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
    const char *test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    REQUIRE (test_srcdir);

    std::ifstream inputFile (std::string (test_srcdir) + "/data/resource/grugs/tiny.graphml");
    REQUIRE (inputFile.is_open ());

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

}  // namespace Flux::resource_model::detail
