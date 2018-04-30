#!/bin/bash

flux module load sched

size=$(flux getattr size)
NJOBS=$1
echo FLUX_URI=$FLUX_URI

for i in `seq 1 $NJOBS`; do
    flux wreckrun --ntasks $size /bin/true
done

flux module remove sched
