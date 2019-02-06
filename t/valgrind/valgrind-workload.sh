#!/bin/bash

echo FLUX_URI=$FLUX_URI
TESTDIR=${SHARNESS_TEST_SRCDIR:-.}/valgrind/workload.d/*
exitcode=0

# Check for no workload:
test -d ${TESTDIR} || exit $exitcode

for file in ${TESTDIR}/*; do
	echo Running $(basename $file)
	$file
	rc=$?
	if test $rc -gt 0; then
		echo "$(basename $file): Failed with rc=$rc" >&2
		exitcode=1
	fi
done
exit $exitcode
