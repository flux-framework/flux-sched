#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c++/reapi_cli.hpp>
#include <fstream>

namespace Flux::resource_model::detail {

TEST_CASE ("Initialize REAPI CLI", "[initialize C++]")
{
    const std::string options = "{}";
    std::stringstream buffer;
    std::ifstream inputFile ("../../../t/data/resource/jgfs/tiny.json");

    if (!inputFile.is_open ()) {
        std::cerr << "Error opening file!" << std::endl;
    }

    buffer << inputFile.rdbuf ();
    std::string rgraph = buffer.str ();

    std::shared_ptr<resource_query_t> ctx = nullptr;
    ctx = std::make_shared<resource_query_t> (rgraph, options);
    REQUIRE (ctx);
}

TEST_CASE ("Match and basic jobspec", "[match grow C++]")
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
    REQUIRE (rc == 0);
    REQUIRE (reserved == false);
    REQUIRE (at == 0);

    R = "";
    match_op = match_op_t::MATCH_GROW_ALLOCATION;
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
}

TEST_CASE ("Match shrink basic jobspec", "[match shrink C++]")
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
    REQUIRE (rc == 0);
    REQUIRE (reserved == false);
    REQUIRE (at == 0);

    bool noent_ok = false;
    bool full_removal = true;
    rc = detail::reapi_cli_t::cancel (ctx.get (), jobid, cancel_string, noent_ok, full_removal);
    REQUIRE (rc == 0);
    REQUIRE (full_removal == false);
}

}  // namespace Flux::resource_model::detail
