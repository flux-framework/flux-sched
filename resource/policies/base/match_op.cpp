#include "match_op.h"
#include <cstring>
#include <array>
#include <algorithm>

constexpr std::array<std::pair<match_op_t, const char *>, (int)(END_MATCH_OP_T)-1> opt_map =
    {std::pair (MATCH_UNKNOWN, "error"),
     std::pair (MATCH_ALLOCATE, "allocate"),
     std::pair (MATCH_ALLOCATE_ORELSE_RESERVE, "allocate_orelse_reserve"),
     std::pair (MATCH_ALLOCATE_W_SATISFIABILITY, "allocate_with_satisfiability"),
     std::pair (MATCH_SATISFIABILITY, "satisfiability")};

const char *match_op_to_string (match_op_t match_op)
{
    for (const auto p : opt_map)
        if (p.first == match_op)
            return p.second;

    return "error";
}

match_op_t string_to_match_op (const char *str)
{
    for (const auto p : opt_map)
        if (strcmp (p.second, str) == 0)
            return p.first;

    return MATCH_UNKNOWN;
}

bool match_op_valid (match_op_t match_op)
{
    return match_op != MATCH_UNKNOWN
           && count_if (opt_map.begin (), opt_map.end (), [match_op] (auto p) {
                  return p.first == match_op;
              }) == 1;
}
