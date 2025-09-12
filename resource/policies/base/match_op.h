#ifndef MATCH_OP_H
#define MATCH_OP_H

typedef enum match_op_t {
    MATCH_UNKNOWN,
    MATCH_ALLOCATE,
    MATCH_ALLOCATE_W_SATISFIABILITY,
    MATCH_ALLOCATE_ORELSE_RESERVE,
    MATCH_SATISFIABILITY,
    MATCH_WITHOUT_ALLOCATING,
    MATCH_WITHOUT_ALLOCATING_EXTEND
} match_op_t;

static const char *match_op_to_string (match_op_t match_op)
{
    switch (match_op) {
        case MATCH_ALLOCATE:
            return "allocate";
        case MATCH_ALLOCATE_ORELSE_RESERVE:
            return "allocate_orelse_reserve";
        case MATCH_ALLOCATE_W_SATISFIABILITY:
            return "allocate_with_satisfiability";
        case MATCH_SATISFIABILITY:
            return "satisfiability";
        case MATCH_WITHOUT_ALLOCATING:
            return "without_allocating";
        case MATCH_WITHOUT_ALLOCATING_EXTEND:
            return "without_allocating_extend";
        default:
            return "error";
    }
}

static const match_op_t string_to_match_op (const char *str)
{
    if (strcmp (str, "allocate") == 0)
        return MATCH_ALLOCATE;
    else if (strcmp (str, "allocate_with_satisfiability") == 0)
        return MATCH_ALLOCATE_W_SATISFIABILITY;
    else if (strcmp (str, "allocate_orelse_reserve") == 0)
        return MATCH_ALLOCATE_ORELSE_RESERVE;
    else if (strcmp (str, "satisfiability") == 0)
        return MATCH_SATISFIABILITY;
    else if (strcmp (str, "without_allocating") == 0)
        return MATCH_WITHOUT_ALLOCATING;
    else if (strcmp (str, "without_allocating_extend") == 0)
        return MATCH_WITHOUT_ALLOCATING_EXTEND;
    else
        return MATCH_UNKNOWN;
}

static bool match_op_valid (match_op_t match_op)
{
    if ((match_op != MATCH_ALLOCATE) && (match_op != MATCH_ALLOCATE_W_SATISFIABILITY)
        && (match_op != MATCH_ALLOCATE_ORELSE_RESERVE) && (match_op != MATCH_SATISFIABILITY)
        && (match_op != MATCH_WITHOUT_ALLOCATING)
        && (match_op != MATCH_WITHOUT_ALLOCATING_EXTEND)) {
        return false;
    }

    return true;
}

#endif  // MATCH_OP_H
