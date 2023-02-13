/*****************************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

package main

import (
	"flag"
	"fluxcli"
	"fmt"
	"io/ioutil"
)

func main() {
	ctx := fluxcli.NewReapiCli()
	jgfPtr := flag.String("jgf", "", "path to jgf")
	jobspecPtr := flag.String("jobspec", "", "path to jobspec")
	reserve := flag.Bool("reserve", false, "or else reserve?")
	flag.Parse()

	jgf, err := ioutil.ReadFile(*jgfPtr)
	if err != nil {
		fmt.Println("Error reading JGF file")
		return
	}
	fluxerr := fluxcli.ReapiCliInit(ctx, string(jgf), "{}")
	if fluxerr < 0 {
		fmt.Println("Error init ReapiCli")
		return
	}
	fmt.Printf("Errors so far: %s\n", fluxcli.ReapiCliGetErrMsg(ctx))

	jobspec, err := ioutil.ReadFile(*jobspecPtr)
	if err != nil {
		fmt.Println("Error reading jobspec file")
		return
	}
	fmt.Printf("Jobspec:\n %s\n", jobspec)

	reserved, allocated, at, overhead, jobid, fluxerr := fluxcli.ReapiCliMatchAllocate(ctx, *reserve, string(jobspec))
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliMatchAllocate")
		return
	}
	printOutput(reserved, allocated, at, jobid, fluxerr)
	reserved, allocated, at, overhead, jobid, fluxerr = fluxcli.ReapiCliMatchAllocate(ctx, *reserve, string(jobspec))
	fmt.Println("Errors so far: \n", fluxcli.ReapiCliGetErrMsg(ctx))

	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliMatchAllocate")
		return
	}
	printOutput(reserved, allocated, at, jobid, fluxerr)
	fluxerr = fluxcli.ReapiCliCancel(ctx, 1, false)
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliCancel")
		return
	}
	fmt.Printf("Cancel output: %d\n", fluxerr)

	reserved, at, overhead, mode, fluxerr := fluxcli.ReapiCliInfo(ctx, 1)
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliInfo")
		return
	}
	fmt.Printf("Info output jobid 1: %t, %d, %f, %s, %d\n", reserved, at, overhead, mode, fluxerr)

	reserved, at, overhead, mode, fluxerr = fluxcli.ReapiCliInfo(ctx, 2)
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliInfo")
		return
	}
	fmt.Printf("Info output jobid 2: %t, %d, %f, %d\n", reserved, at, overhead, fluxerr)

}

func printOutput(reserved bool, allocated string, at int64, jobid uint64, fluxerr int) {
	fmt.Println("\n\t----Match Allocate output---")
	fmt.Printf("jobid: %d\nreserved: %t\nallocated: %s\nat: %d\nerror: %d\n", jobid, reserved, allocated, at, fluxerr)
}
