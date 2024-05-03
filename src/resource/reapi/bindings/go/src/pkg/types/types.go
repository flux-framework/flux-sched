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
#include "resource/reapi/bindings/c/reapi_cli.h"

/* usage:
mt := MatchType
C.match_op_t(mt)
*/

import "C"

type MatchType int

// MatchUnknown serves as a sentinel value that it's undefined
const (
	MatchUnknown                    MatchType = iota // unknown
	MatchAllocate                                    // allocate
	MatchAllocateWithSatisfiability                  // allocate with satisfiability
	MatchAllocateOrElseReserve                       // allocate or else reserve
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
