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

TEST_CASE ("Initialize REAPI CLI and test match, satisfy, and cancel",
           "[initialize-match-satisfy-cancel C]")
{
    std::string options = "{\"load_format\": \"grug\"}";

    const char *test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    REQUIRE (test_srcdir);

    std::stringstream buffer, buffer2, buffer3;
    std::ifstream resource_file (std::string (test_srcdir) + "/data/resource/grugs/tiny.graphml");
    std::ifstream jobspec_file (std::string (test_srcdir)
                                + "/data/resource/jobspecs/basics/test001.yaml");
    std::ifstream jobspec_file2 (std::string (test_srcdir)
                                 + "/data/resource/jobspecs/basics/test011.yaml");
    match_op_t match_op = match_op_t::MATCH_ALLOCATE;
    uint64_t jobid, jobid2, jobid3;
    bool reserved, satisfiable;
    char *R = nullptr, *mode = nullptr;
    int64_t at;
    double ov;

    REQUIRE (resource_file.is_open () == true);
    buffer << resource_file.rdbuf ();
    const std::string rgraph = buffer.str ();

    REQUIRE (jobspec_file.is_open () == true);
    buffer2 << jobspec_file.rdbuf ();
    const std::string jobspec = buffer2.str ();

    REQUIRE (jobspec_file2.is_open () == true);
    buffer3 << jobspec_file2.rdbuf ();
    const std::string jobspec2 = buffer3.str ();

    reapi_cli_ctx_t *ctx = reapi_cli_new ();

    // Test initialize with invalid params
    REQUIRE (reapi_cli_initialize (ctx, "", "") != 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test match with empty resource graph
    REQUIRE (reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov)
             != 0);

    // Test initialization with valid params
    REQUIRE (reapi_cli_initialize (ctx, rgraph.c_str (), options.c_str ()) == 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test match with populated resource graph
    CHECK (reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid2, &reserved, &R, &at, &ov)
           == 0);
    CHECK (reserved == false);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test match_allocate with orelse_reserve = true
    CHECK (reapi_cli_match_allocate (ctx, true, jobspec.c_str (), &jobid3, &reserved, &R, &at, &ov)
           == 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test satisfy
    CHECK (reapi_cli_match_satisfy (ctx, jobspec.c_str (), &satisfiable, &ov) == 0);
    CHECK (satisfiable == true);

    // Test satisfy with unsatisfiable jobspec
    CHECK (reapi_cli_match_satisfy (ctx, jobspec2.c_str (), &satisfiable, &ov) != 0);
    CHECK (satisfiable == false);

    // Test info with valid jobid
    CHECK (reapi_cli_info (ctx, jobid2, &mode, &reserved, &at, &ov) == 0);
    CHECK (std::string (mode) == "ALLOCATED");

    // Test cancel with invalid jobid
    CHECK (reapi_cli_cancel (ctx, jobid3 + 1, false) != 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test cancel with valid jobid
    CHECK (reapi_cli_cancel (ctx, jobid2, false) == 0);
    INFO (reapi_cli_get_err_msg (ctx));

    reapi_cli_destroy (ctx);
}

TEST_CASE ("Initialize REAPI CLI and test add and remove", "[initialize-add-remove C]")
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

    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    REQUIRE (ctx);

    // Operations must fail before initialization (rqt is null).
    REQUIRE (reapi_cli_initialize (ctx, "", "") != 0);
    REQUIRE (reapi_cli_add_subgraph (ctx, add_sgraph.c_str ()) != 0);
    REQUIRE (reapi_cli_remove_subgraph (ctx, remove_path.c_str ()) != 0);

    REQUIRE (reapi_cli_initialize (ctx, rgraph.c_str (), options.c_str ()) == 0);

    // Empty inputs must fail.
    REQUIRE (reapi_cli_add_subgraph (ctx, "") != 0);
    REQUIRE (reapi_cli_remove_subgraph (ctx, "") != 0);

    // Valid add then remove.
    CHECK (reapi_cli_add_subgraph (ctx, add_sgraph.c_str ()) == 0);
    CHECK (reapi_cli_remove_subgraph (ctx, remove_path.c_str ()) == 0);

    reapi_cli_destroy (ctx);
}

// Regression test for the interner scope guard in add_subgraph.
//
// resource_query_t's constructor finalizes the subsystem_t and resource_type_t
// interners after the initial graph load, so adding a subgraph that introduces a
// *new* resource type (or subsystem) must reopen the interner storage across the
// JGF reader's unpack_at call (that is what add_subgraph's open_for_scope() guards
// are for). If the guards are scoped so they are destroyed before unpack_at runs,
// the storage is closed again and interning the new type throws std::system_error
// ("This interner is finalized and must be open to add strings, found new string:
// 'gpu'"), causing the add to fail.
//
// The base graph (node-test.json) contains only cluster/rack/node/socket/core, so
// this subgraph -- which attaches a gpu under node0 -- is the first thing to force
// interning of a genuinely new type. Same-type additions (e.g. node-add-test.json)
// do NOT exercise this path and pass even with a broken guard.
TEST_CASE ("Add subgraph introducing a new resource type", "[add-subgraph-new-type C]")
{
    const std::string options = "{\"load_format\": \"jgf\"}";
    std::stringstream base_buf, add_buf;
    const char *test_srcdir = std::getenv ("SHARNESS_TEST_SRCDIR");
    REQUIRE (test_srcdir);

    // Base graph: no "gpu" type present.
    std::ifstream base_file (std::string (test_srcdir)
                             + "/data/resource/jgfs/elastic/node-test.json");
    REQUIRE (base_file.is_open ());
    base_buf << base_file.rdbuf ();
    const std::string rgraph = base_buf.str ();

    // Subgraph that attaches a gpu (a new type) under /medium0/rack0/node0.
    std::ifstream add_file (std::string (test_srcdir)
                            + "/data/resource/jgfs/elastic/node-add-gpu-test.json");
    REQUIRE (add_file.is_open ());
    add_buf << add_file.rdbuf ();
    const std::string add_sgraph = add_buf.str ();

    reapi_cli_ctx_t *ctx = reapi_cli_new ();
    REQUIRE (ctx);

    REQUIRE (reapi_cli_initialize (ctx, rgraph.c_str (), options.c_str ()) == 0);

    // The assertion that distinguishes a correct guard from the buggy one: adding a
    // subgraph with a new type must succeed. With the guard improperly scope this either
    // returns non-zero or lets the interner's std::system_error escape, and the test
    // fails either way.
    int rc = reapi_cli_add_subgraph (ctx, add_sgraph.c_str ());
    INFO ("reapi_cli_add_subgraph rc=" << rc << " err='" << reapi_cli_get_err_msg (ctx) << "'");
    CHECK (rc == 0);

    reapi_cli_destroy (ctx);
}

}  // namespace Flux::resource_model::detail
