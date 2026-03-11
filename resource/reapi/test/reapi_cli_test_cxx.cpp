#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c++/reapi_cli.hpp>
#include <resource/policies/base/match_op.h>
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

}  // namespace Flux::resource_model::detail
