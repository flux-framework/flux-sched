#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c/reapi_cli.h>
#include <fstream>
#include <iostream>

namespace Flux::resource_model::detail {

TEST_CASE ("Initialize REAPI CLI and test match, satisfy, and cancel",
           "[initialize-match-satisfy-cancel]")
{
    std::string options = "{\"load_format\": \"grug\"}";
    std::stringstream buffer, buffer2, buffer3;
    std::ifstream resource_file ("../../../t/data/resource/grugs/tiny.graphml");
    std::ifstream jobspec_file ("../../../t/data/resource/jobspecs/basics/test001.yaml");
    std::ifstream jobspec_file2 ("../../../t/data/resource/jobspecs/basics/test011.yaml");
    match_op_t match_op = match_op_t::MATCH_ALLOCATE;
    uint64_t jobid, jobid2, jobid3;
    bool reserved, satisfiable;
    char *R, *mode;
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
    CHECK (reapi_cli_match_satisfy (ctx, jobspec2.c_str (), &satisfiable, &ov) == 0);
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

TEST_CASE ("Initialize REAPI CLI and test add and remove", "[initialize-add-remove]")
{
    const std::string options = "{\"load_format\": \"jgf\"}";
    std::stringstream buffer, buffer2;
    std::ifstream resource_file ("../../../t/data/resource/jgfs/elastic/node-test.json");
    std::ifstream add_file ("../../../t/data/resource/jgfs/elastic/node-add-test.json");
    std::string remove_path = "/medium0/rack0/node0";

    REQUIRE (resource_file.is_open () == true);
    buffer << resource_file.rdbuf ();
    std::string rgraph = buffer.str ();

    REQUIRE (add_file.is_open () == true);
    buffer2 << add_file.rdbuf ();
    std::string add_sgraph = buffer2.str ();

    reapi_cli_ctx_t *ctx = reapi_cli_new ();

    // Test initialize with invalid params
    REQUIRE (reapi_cli_initialize (ctx, "", "") != 0);

    // Test add_subgraph with empty resource graph
    REQUIRE (reapi_cli_add_subgraph (ctx, add_sgraph.c_str ()) != 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test remove_subgraph with empty resource graph
    REQUIRE (reapi_cli_remove_subgraph (ctx, remove_path.c_str ()) != 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test initialize with valid params
    REQUIRE (reapi_cli_initialize (ctx, rgraph.c_str (), options.c_str ()) == 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test add_subgraph with invalid params
    REQUIRE (reapi_cli_add_subgraph (ctx, "") != 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test add_subgraph with valid params
    CHECK (reapi_cli_add_subgraph (ctx, add_sgraph.c_str ()) == 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test remove_subgraph with invalid params
    REQUIRE (reapi_cli_remove_subgraph (ctx, "") != 0);
    INFO (reapi_cli_get_err_msg (ctx));

    // Test remove_subgraph with valid params
    CHECK (reapi_cli_remove_subgraph (ctx, remove_path.c_str ()) == 0);
    INFO (reapi_cli_get_err_msg (ctx));

    reapi_cli_destroy (ctx);
}

}  // namespace Flux::resource_model::detail
