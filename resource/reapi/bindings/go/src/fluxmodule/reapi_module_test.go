/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

package fluxmodule

import (
	"fmt"
	"os"
	"testing"
)

func TestFluxmodule(t *testing.T) {
	mod := NewReapiModule()
	jobspec, err := os.ReadFile("/root/flux-sched/t/data/resource/jobspecs/basics/test001.yaml")
	if !mod.HasContext() {
		t.Errorf("Context is null")
	}
	reserved, allocated, at, overhead, err1 := mod.MatchAllocate(false, string(jobspec), 4)
	fmt.Printf("%t, %s, %d, %f, %d, %s", reserved, allocated, at, overhead, err1, err)

}
