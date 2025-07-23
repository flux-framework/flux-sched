#ifndef MATCH_OP_H
#define MATCH_OP_H

typedef enum match_op_t {
    MATCH_UNKNOWN,
    MATCH_ALLOCATE,
    MATCH_ALLOCATE_W_SATISFIABILITY,
    MATCH_ALLOCATE_ORELSE_RESERVE,
    MATCH_GROW_ALLOCATION,
    MATCH_SATISFIABILITY
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
        case MATCH_GROW_ALLOCATION:
            return "grow_allocation";
        case MATCH_SATISFIABILITY:
            return "satisfiability";
        default:
            return "error";
    }
}

static bool match_op_valid (match_op_t match_op)
{
    if ((match_op != MATCH_ALLOCATE) && (match_op != MATCH_ALLOCATE_W_SATISFIABILITY)
        && (match_op != MATCH_ALLOCATE_ORELSE_RESERVE) && (match_op != MATCH_SATISFIABILITY)
        && (match_op != MATCH_GROW_ALLOCATION)) {
        return false;
    }

    return true;
}

#endif  // MATCH_OP_H
