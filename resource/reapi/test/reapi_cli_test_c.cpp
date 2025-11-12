#define CATCH_CONFIG_MAIN

#include <catch2/catch_test_macros.hpp>
#include <resource/reapi/bindings/c/reapi_cli.h>
#include <fstream>
#include <iostream>
#include <jansson.h>

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

    reapi_cli_ctx_t *ctx = nullptr;
    ctx = reapi_cli_new ();
    REQUIRE (ctx);

    rc = reapi_cli_initialize (ctx, rgraph.c_str (), options.c_str ());
    REQUIRE (rc == 0);

    SECTION ("MATCH_ALLOCATE")
    {
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

    SECTION ("MATCH_WITHOUT_ALLOCATING")
    {
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

    SECTION ("MATCH_WITHOUT_ALLOCATING_EXTEND")
    {
        bool reserved = false;
        char *R;
        uint64_t jobid = 1;
        uint64_t jobids_to_cancel[8];
        double ov = 0.0;
        int64_t at = 0;
        int64_t expiration;
        const json_t *match_json;
        const json_t *exec_json;
        const json_t *exp_json;

        // This section is easiest to understand visually.
        // Let A=Allocated, R=Reserved, and M=Matched.
        // Each row is a representation of a node's planner over time.
        // To start, there are no allocations/reservations/matches.
        // 1:
        // 2:
        // 3:
        // 4:
        // t: 0    3600 7200

        // MWOAE extends an allocation to graph_end if possible
        // 1: M    M    M...
        // 2:
        // 3:
        // 4:
        // t: 0    3600 7200
        match_op_t match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_EXTEND;
        rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
        CHECK (reserved == false);
        CHECK (at == 0);
        REQUIRE (rc == 0);
        CHECK ((match_json = json_loads (R, JSON_DECODE_ANY, NULL)) != nullptr);
        CHECK ((exec_json = json_object_get (match_json, "execution")) != nullptr);
        CHECK ((exp_json = json_object_get (exec_json, "expiration")) != nullptr);
        REQUIRE ((expiration = json_integer_value (exp_json)) == 0x7FFFFFFFFFFFFFFF);

        // Temporarily allocate all resources
        // 1: A
        // 2: A
        // 3: A
        // 4: A
        // t: 0    3600 7200
        match_op = match_op_t::MATCH_ALLOCATE;
        for (int i = 0; i < 4; i++) {
            rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
            CHECK (reserved == false);
            CHECK (at == 0);
            REQUIRE (rc == 0);
            jobids_to_cancel[i] = jobid;
        }

        // MWOAE looks for a start time after previous allocations (t=3600)
        // 1: A    M    M...
        // 2: A
        // 3: A
        // 4: A
        // t: 0    3600 7200
        match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_EXTEND;
        rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
        CHECK (reserved == false);
        CHECK (at == 3600);
        REQUIRE (rc == 0);
        CHECK ((match_json = json_loads (R, JSON_DECODE_ANY, NULL)) != nullptr);
        CHECK ((exec_json = json_object_get (match_json, "execution")) != nullptr);
        CHECK ((exp_json = json_object_get (exec_json, "expiration")) != nullptr);
        REQUIRE ((expiration = json_integer_value (exp_json)) == 0x7FFFFFFFFFFFFFFF);

        // Temporarily reserve resources in the future (t=3600)
        // 1: A    R
        // 2: A    R
        // 3: A    R
        // 4: A    R
        // t: 0    3600 7200
        match_op = match_op_t::MATCH_ALLOCATE_ORELSE_RESERVE;
        for (int i = 0; i < 4; i++) {
            rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
            CHECK (reserved == true);
            CHECK (at == 3600);
            REQUIRE (rc == 0);
            jobids_to_cancel[4 + i] = jobid;
        }

        // Permanently reserve a resource in the future (t=7200)
        // 1: A    R    R
        // 2: A    R
        // 3: A    R
        // 4: A    R
        // t: 0    3600 7200
        rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
        CHECK (reserved == true);
        CHECK (at == 7200);
        REQUIRE (rc == 0);

        // Cancel temporary allocations/reservations at t=0 and t=3600
        // 1:           R
        // 2:
        // 3:
        // 4:
        // t: 0    3600 7200
        for (int i = 0; i < 8; i++)
            REQUIRE ((rc = reapi_cli_cancel (ctx, jobids_to_cancel[i], false)) == 0);

        // MWOAE extends an allocation to the next allocation/reservation,
        // which happens to be the permanent reservation at t=7200 in this
        // case since policy=first
        // 1: M    M    R
        // 2:
        // 3:
        // 4:
        // t: 0    3600 7200
        match_op = match_op_t::MATCH_WITHOUT_ALLOCATING_EXTEND;
        rc = reapi_cli_match (ctx, match_op, jobspec.c_str (), &jobid, &reserved, &R, &at, &ov);
        CHECK (reserved == false);
        CHECK (at == 0);
        REQUIRE (rc == 0);
        CHECK ((match_json = json_loads (R, JSON_DECODE_ANY, NULL)) != nullptr);
        CHECK ((exec_json = json_object_get (match_json, "execution")) != nullptr);
        CHECK ((exp_json = json_object_get (exec_json, "expiration")) != nullptr);
        REQUIRE ((expiration = json_integer_value (exp_json)) == 7200);
    }
}

}  // namespace Flux::resource_model::detail
