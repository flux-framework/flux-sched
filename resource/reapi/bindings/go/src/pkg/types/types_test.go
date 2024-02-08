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

import (
	"testing"
)

// List of match types, purposefully out of order
var matchTypes = []MatchType{
	MatchAllocateWithSatisfiability,
	MatchAllocateOrElseReserve,
	MatchAllocate,
	MatchSatisfiability,
	MatchUnknown,
}

func TestToString(t *testing.T) {
	type test struct {
		description string
		input       MatchType
		expected    string
	}

	tests := []test{
		{description: "unknown", input: MatchUnknown, expected: "unknown"},
		{description: "allocate", input: MatchAllocate, expected: "allocate"},
		{description: "satisfiability", input: MatchSatisfiability, expected: "satisfiability"},
		{description: "allocate or else reserve", input: MatchAllocateOrElseReserve, expected: "allocate or else reserve"},
		{description: "allocate with satisfiability", input: MatchAllocateWithSatisfiability, expected: "allocate with satisfiability"},
	}
	for _, item := range tests {
		t.Run(item.description, func(t *testing.T) {
			value := item.input.String()
			t.Logf("got %s, want %s", value, item.expected)
			if item.expected != value {
				t.Errorf("got %s, want %s", value, item.expected)
			}
		})
	}
}

func TestAsInt(t *testing.T) {
	type test struct {
		description string
		input       MatchType
		expected    int
	}

	tests := []test{
		{description: "unknown", input: MatchUnknown, expected: 0},
		{description: "allocate", input: MatchAllocate, expected: 1},
		{description: "satisfiability", input: MatchSatisfiability, expected: 4},
		{description: "allocate or else reserve", input: MatchAllocateOrElseReserve, expected: 3},
		{description: "allocate with satisfiability", input: MatchAllocateWithSatisfiability, expected: 2},
	}
	for _, item := range tests {
		t.Run(item.description, func(t *testing.T) {
			value := int(item.input)
			t.Logf("got %d, want %d", value, item.expected)
			if item.expected != value {
				t.Errorf("got %d, want %d", value, item.expected)
			}
		})
	}
}
