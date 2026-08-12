/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

package fluxcli

import "testing"

// A minimal resource graph: one cluster -> one node -> one core.
const tinyJGF = `{"graph": {"nodes": [` +
	`{"id": "0", "metadata": {"type": "cluster", "basename": "tiny", "name": "tiny0", "size": 1, "paths": {"containment": "/tiny0"}}},` +
	`{"id": "1", "metadata": {"type": "node", "basename": "node", "name": "node0", "size": 1, "rank": 0, "paths": {"containment": "/tiny0/node0"}}},` +
	`{"id": "2", "metadata": {"type": "core", "basename": "core", "name": "core0", "size": 1, "id": 0, "rank": 0, "paths": {"containment": "/tiny0/node0/core0"}}}` +
	`], "edges": [{"source": "0", "target": "1"}, {"source": "1", "target": "2"}]}}`

const tinyParams = `{"load_format": "jgf", "matcher_policy": "high", "match_format": "rv1", "matcher_name": "CA"}`

// Fits the graph exactly (1 core): satisfiable.
const satisfiableJobspec = `{"version": 1, "resources": [{"type": "node", "count": 1, "with": [` +
	`{"type": "slot", "count": 1, "label": "task", "with": [{"type": "core", "count": 1}]}]}],` +
	`"tasks": [{"command": ["sleep", "0"], "slot": "task", "count": {"per_slot": 1}}],` +
	`"attributes": {"system": {"duration": 60.0}}}`

// Valid but asks for more cores than exist: not satisfiable (traverser ENODEV).
const unsatisfiableJobspec = `{"version": 1, "resources": [{"type": "node", "count": 1, "with": [` +
	`{"type": "slot", "count": 1, "label": "task", "with": [{"type": "core", "count": 2}]}]}],` +
	`"tasks": [{"command": ["app"], "slot": "task", "count": {"per_slot": 1}}],` +
	`"attributes": {"system": {"duration": 60.0}}}`

// References a resource type absent from the graph: a genuine error, not a
// plain "not satisfiable" result (traverser errno is not ENODEV).
const errorJobspec = `{"version": 1, "resources": [{"type": "node", "count": 1, "with": [` +
	`{"type": "slot", "count": 1, "label": "task", "with": [{"type": "core", "count": 1}, {"type": "mpi", "count": 1}]}]}],` +
	`"tasks": [{"command": ["app"], "slot": "task", "count": {"per_slot": 1}}],` +
	`"attributes": {"system": {"duration": 60.0}}}`

func TestFluxcli(t *testing.T) {
	cli := NewReapiClient()
	if !cli.HasContext() {
		t.Errorf("Context is null")
	}
}

// TestMatchSatisfy exercises the three-way result of MatchSatisfy: 0
// (satisfiable), 1 (not satisfiable), and -1 (error).
func TestMatchSatisfy(t *testing.T) {
	cli := NewReapiClient()
	if err := cli.InitContext(tinyJGF, tinyParams); err != nil {
		t.Fatalf("InitContext failed: %v", err)
	}

	// Satisfiable request -> 0 with a nil error.
	if code, _, err := cli.MatchSatisfy(satisfiableJobspec); code != 0 || err != nil {
		t.Errorf("satisfiable jobspec: want (0, nil), got (%d, %v)", code, err)
	}

	// Unsatisfiable request -> 1 with a nil error (not a failure).
	if code, _, err := cli.MatchSatisfy(unsatisfiableJobspec); code != 1 || err != nil {
		t.Errorf("unsatisfiable jobspec: want (1, nil), got (%d, %v)", code, err)
	}

	// Genuine error -> -1 with a non-nil error.
	if code, _, err := cli.MatchSatisfy(errorJobspec); code != -1 || err == nil {
		t.Errorf("bad jobspec: want (-1, non-nil error), got (%d, %v)", code, err)
	}
}
