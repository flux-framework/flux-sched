#ifndef MATCH_OP_H
#define MATCH_OP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum match_op_t {
    START_MATCH_OP_T = 0,
    MATCH_UNKNOWN,
    MATCH_ALLOCATE,
    MATCH_ALLOCATE_W_SATISFIABILITY,
    MATCH_ALLOCATE_ORELSE_RESERVE,
    MATCH_SATISFIABILITY,
    MATCH_WITHOUT_ALLOCATING,
    END_MATCH_OP_T
} match_op_t;

const char *match_op_to_string (match_op_t match_op);

const match_op_t string_to_match_op (const char *str);

bool match_op_valid (match_op_t match_op);

#ifdef __cplusplus
}
#endif

#endif  // MATCH_OP_H
