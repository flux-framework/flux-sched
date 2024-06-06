#!/bin/bash

echo FLUX_URI=$FLUX_URI
exitcode=0

flux module remove -f sched-simple
flux module load sched-fluxion-resource load-allowlist=node,core,gpu
flux module load sched-fluxion-qmanager
flux dmesg -H | grep sched-fluxion.*version

for file in ${SHARNESS_TEST_SRCDIR:-..}/valgrind/workload.d/*; do
	echo Running $(basename $file)
	$file
	rc=$?
	if test $rc -gt 0; then
		echo "$(basename $file): Failed with rc=$rc" >&2
		exitcode=1
	fi
done

flux module remove -f sched-fluxion-resource
flux module remove -f sched-fluxion-qmanager
flux module load sched-simple

exit $exitcode
