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
	"fmt"
	"os"

	"github.com/flux-framework/flux-sched/resource/reapi/bindings/go/src/fluxcli"
)

func main() {
	jgfPtr := flag.String("jgf", "", "path to jgf")
	jobspecPtr := flag.String("jobspec", "", "path to jobspec")
	reserve := false
	flag.Parse()

	jgf, err := os.ReadFile(*jgfPtr)
	if err != nil {
		fmt.Println("Error reading JGF file")
		return
	}
	cli := fluxcli.NewReapiClient()
	err = cli.InitContext(string(jgf), "{}")
	if err != nil {
		fmt.Printf("Error initializing jobspec context for ReapiClient: %v\n", err)
		return
	}
	fmt.Printf("Errors so far: %s\n", cli.GetErrMsg())

	jobspec, err := os.ReadFile(*jobspecPtr)
	if err != nil {
		fmt.Printf("Error reading jobspec file: %v\n", err)
		return
	}
	fmt.Printf("Jobspec:\n %s\n", jobspec)

	reserved, allocated, at, overhead, jobid, err := cli.MatchAllocate(reserve, string(jobspec))
	if err != nil {
		fmt.Printf("Error in ReapiClient MatchAllocate: %v\n", err)
		return
	}
	printOutput(reserved, allocated, at, jobid, err)

	reserve = true
	reserved, allocated, at, overhead, jobid, err = cli.MatchAllocate(reserve, string(jobspec))
	fmt.Println("Errors so far: \n", cli.GetErrMsg())

	if err != nil {
		fmt.Printf("Error in ReapiClient MatchAllocate: %v\n", err)
		return
	}
	printOutput(reserved, allocated, at, jobid, err)

	reserved, allocated, at, overhead, jobid, err = cli.Match("allocate", string(jobspec))
	fmt.Println("Errors so far: \n", cli.GetErrMsg())

	if err != nil {
		fmt.Printf("Error in ReapiClient MatchAllocate: %v\n", err)
		return
	}
	printOutput(reserved, allocated, at, jobid, err)

	reserved, allocated, at, overhead, jobid, err = cli.Match("allocate_orelse_reserve", string(jobspec))
	fmt.Println("Errors so far: \n", cli.GetErrMsg())

	if err != nil {
		fmt.Printf("Error in ReapiClient MatchAllocate: %v\n", err)
		return
	}
	printOutput(reserved, allocated, at, jobid, err)

	reserved, allocated, at, overhead, jobid, err = cli.Match("allocate_with_satisfiability", string(jobspec))
	fmt.Println("Errors so far: \n", cli.GetErrMsg())

	if err != nil {
		fmt.Printf("Error in ReapiClient MatchAllocate: %v\n", err)
		return
	}
	printOutput(reserved, allocated, at, jobid, err)

	sat, overhead, err := cli.MatchSatisfy(string(jobspec))
	fmt.Println("Errors so far: \n", cli.GetErrMsg())

	if err != nil {
		fmt.Printf("Error in ReapiClient MatchAllocate: %v\n", err)
		return
	}
	printSatOutput(sat, err)

	err = cli.Cancel(1, false)
	if err != nil {
		fmt.Printf("Error in ReapiClient Cancel: %v\n", err)
		return
	}
	fmt.Printf("Cancel output: %v\n", err)

	reserved, at, overhead, mode, err := cli.Info(1)
	if err != nil {
		fmt.Printf("Error in ReapiClient Info: %v\n", err)
		return
	}
	fmt.Printf("Info output jobid 1: %t, %d, %f, %s, %v\n", reserved, at, overhead, mode, err)

	reserved, at, overhead, mode, err = cli.Info(2)
	if err != nil {
		fmt.Println("Error in ReapiClient Info: %v\n", err)
		return
	}
	fmt.Printf("Info output jobid 2: %t, %d, %f, %v\n", reserved, at, overhead, err)

}

func printOutput(reserved bool, allocated string, at int64, jobid uint64, err error) {
	fmt.Println("\n\t----Match Allocate output---")
	fmt.Printf("jobid: %d\nreserved: %t\nallocated: %s\nat: %d\nerror: %v\n", jobid, reserved, allocated, at, err)
}

func printSatOutput(sat bool, err error) {
	fmt.Println("\n\t----Match Satisfy output---")
	fmt.Printf("satisfied: %t\nerror: %v\n", sat, err)
}
