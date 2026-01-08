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
    const std::string options = "{\"match_format\": \"rv1\"}";
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

    SECTION ("MATCH_ALLOCATE")
    {
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

    SECTION ("MATCH_WITHOUT_ALLOCATING")
    {
        bool reserved = false;
        std::string R = "";
        uint64_t jobid = 1;
        double ov = 0.0;
        int64_t at = 0;

        match_op_t match_op = match_op_t::MATCH_WITHOUT_ALLOCATING;

        rc =
            reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);
        CHECK (reserved == false);
        CHECK (at == 0);
        REQUIRE (rc == 0);
        jobid = ctx->get_job_counter ();

        match_op = match_op_t::MATCH_ALLOCATE;

        for (int i = 0; i < 4; i++) {
            rc = reapi_cli_t::match_allocate (ctx.get (),
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
            jobid = ctx->get_job_counter ();
        }
        rc =
            reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);
        REQUIRE (rc == -1);  // The tiny graph should be full

        match_op = match_op_t::MATCH_WITHOUT_ALLOCATING;

        rc =
            reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);
        CHECK (reserved == false);
        CHECK (at == 3600);  // MWOA should match the next available time
        CHECK (rc == 0);
    }

    SECTION ("MATCH_WITHOUT_ALLOCATING_EXTEND")
    {
        // For a visual explanation of this section, see reapi_cli_test_c.cpp
        bool reserved = false;
        std::string R = "";
        uint64_t jobid = 1;
        uint64_t jobids_to_cancel[8];
        double ov = 0.0;
        int64_t at = 0;
        int64_t expiration;
        const json_t *match_json;
        const json_t *exec_json;
        const json_t *exp_json;

        // MWOAE extends an allocation to graph_end if possible
        match_op_t match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_EXTEND;
        rc =
            reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);
        CHECK (reserved == false);
        CHECK (at == 0);
        REQUIRE (rc == 0);
        CHECK ((match_json = json_loads (R.c_str (), JSON_DECODE_ANY, NULL)) != nullptr);
        CHECK ((exec_json = json_object_get (match_json, "execution")) != nullptr);
        CHECK ((exp_json = json_object_get (exec_json, "expiration")) != nullptr);
        REQUIRE ((expiration = json_integer_value (exp_json)) == 0x7FFFFFFFFFFFFFFF);

        // Temporarily allocate all resources
        match_op = match_op_t::MATCH_ALLOCATE;
        for (int i = 0; i < 4; i++) {
            rc = reapi_cli_t::match_allocate (ctx.get (),
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
            jobids_to_cancel[i] = jobid;
            jobid = ctx->get_job_counter ();
        }

        // MWOAE looks for a start time after previous allocations (t=3600)
        match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_EXTEND;
        rc =
            reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);
        CHECK (reserved == false);
        CHECK (at == 3600);
        REQUIRE (rc == 0);
        CHECK ((match_json = json_loads (R.c_str (), JSON_DECODE_ANY, NULL)) != nullptr);
        CHECK ((exec_json = json_object_get (match_json, "execution")) != nullptr);
        CHECK ((exp_json = json_object_get (exec_json, "expiration")) != nullptr);
        REQUIRE ((expiration = json_integer_value (exp_json)) == 0x7FFFFFFFFFFFFFFF);
        jobid = ctx->get_job_counter ();

        // Temporarily reserve resources in the future (t=3600)
        match_op = match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE;
        for (int i = 0; i < 4; i++) {
            rc = reapi_cli_t::match_allocate (ctx.get (),
                                              match_op,
                                              jobspec,
                                              jobid,
                                              reserved,
                                              R,
                                              at,
                                              ov);
            CHECK (reserved == true);
            CHECK (at == 3600);
            REQUIRE (rc == 0);
            jobids_to_cancel[4 + i] = jobid;
            jobid = ctx->get_job_counter ();
        }

        // Permanently reserve a resource in the future (t=7200)
        rc =
            reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);
        CHECK (reserved == true);
        CHECK (at == 7200);
        REQUIRE (rc == 0);
        jobid = ctx->get_job_counter ();

        // Cancel temporary allocations/reservations at t=0 and t=3600
        for (int i = 0; i < 8; i++)
            REQUIRE ((rc = reapi_cli_t::cancel (ctx.get (), jobids_to_cancel[i], false)) == 0);

        // MWOAE extends an allocation to the next allocation/reservation,
        // which happens to be the permanent reservation at t=7200 in this
        // case since policy=first
        match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_EXTEND;
        rc =
            reapi_cli_t::match_allocate (ctx.get (), match_op, jobspec, jobid, reserved, R, at, ov);
        CHECK (reserved == false);
        CHECK (at == 0);
        REQUIRE (rc == 0);
        CHECK ((match_json = json_loads (R.c_str (), JSON_DECODE_ANY, NULL)) != nullptr);
        CHECK ((exec_json = json_object_get (match_json, "execution")) != nullptr);
        CHECK ((exp_json = json_object_get (exec_json, "expiration")) != nullptr);
        REQUIRE ((expiration = json_integer_value (exp_json)) == 7200);
    }
}

}  // namespace Flux::resource_model::detail
