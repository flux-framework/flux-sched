#!/bin/bash

flux module load sched

size=$(flux getattr size)
NJOBS=$1
echo FLUX_URI=$FLUX_URI

for i in `seq 1 $NJOBS`; do
    flux wreckrun --ntasks $size /bin/true
done

# Wait up to 5s for last job to be fully complete before removing sched module:
KEY=$(echo $(flux wreck last-jobid -p).state)
${SHARNESS_TEST_SRCDIR}/scripts/kvs-watch-until.lua -t 5 $KEY 'v == "complete"'

flux module remove sched
