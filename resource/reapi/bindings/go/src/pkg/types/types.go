package types

/*****************************************************************************\
 * Copyright 2024 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

/*
#include "reapi_cli.h"

typedef enum {
	MATCH_UNKNOWN = 0
	MATCH_ALLOCATE = 1
	MATCH_ALLOCATE_ORELSE_RESERVE = 2
	MATCH_ALLOCATE_W_SATISFIABILITY = 3
	MATCH_SATISFIABILITY = 4
} match_op_enum;

/* usage:
mt := MatchType
C.match_op_enum(mt)
*/

import "C"

type MatchType int

// MatchUnknown serves as a sentinal value that it's undefined
const (
	MatchUnknown                    MatchType = iota // unknown
	MatchAllocate                                    // allocate
	MatchAllocateOrElseReserve                       // allocate or else reserve
	MatchAllocateWithSatisfiability                  // allocate with satisfiability
	MatchSatisfiability                              // satisfiability
)

// String ensures that the MatchType can be used in string formatting
func (m MatchType) String() string {
	switch m {
	case MatchAllocate:
		return "allocate"
	case MatchAllocateOrElseReserve:
		return "allocate or else reserve"
	case MatchAllocateWithSatisfiability:
		return "allocate with satisfiability"
	case MatchSatisfiability:
		return "satisfiability"
	}
	return "unknown"
}
