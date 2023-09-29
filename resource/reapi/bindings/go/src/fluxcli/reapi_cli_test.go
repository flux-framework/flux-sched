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

func TestFluxcli(t *testing.T) {
	cli := NewReapiClient()
	if !cli.HasContext() {
		t.Errorf("Context is null")
	}

}
