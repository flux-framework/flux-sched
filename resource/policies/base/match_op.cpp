#include "match_op.h"
#include <cstring>
#include <map>

const std::map<match_op_t, const char *> match_options = {{MATCH_UNKNOWN, "error"},
                                                          {MATCH_ALLOCATE, "allocate"},
                                                          {MATCH_ALLOCATE_ORELSE_RESERVE,
                                                           "allocate_orelse_reserve"},
                                                          {MATCH_ALLOCATE_W_SATISFIABILITY,
                                                           "allocate_with_satisfiability"},
                                                          {MATCH_SATISFIABILITY, "satisfiability"}};

const char *match_op_to_string (match_op_t match_op)
{
    std::map<match_op_t, const char *>::const_iterator candidate = match_options.find (match_op);

    if (candidate != match_options.end ())
        return candidate->second;

    return "error";
}

const match_op_t string_to_match_op (const char *str)
{
    for (const auto p : match_options)
        if (strcmp (p.second, str) == 0)
            return p.first;

    return MATCH_UNKNOWN;
}

bool match_op_valid (match_op_t match_op)
{
    return (match_options.count (match_op) == 1) && (match_op != MATCH_UNKNOWN);
}
