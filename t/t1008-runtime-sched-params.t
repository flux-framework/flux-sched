#!/bin/bash

test_description='Test dynamic updation of queue depth. 
Ensure jobs are correctly scheduled under different values of
scheduling optimization parameters and their combinations
'

. `dirname $0`/sharness.sh

basepath=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/hwloc-data`
# each of the 4 brokers manages a full cab node exclusively
excl_4N4B=$basepath/004N/exclusive/04-brokers
excl_4N4B_nc=16

#
# test_under_flux is under sharness.d/
#
test_under_flux 4

#
# print only with --debug
#
test_debug '
    echo ${basepath} &&
    echo ${excl_4N4B} &&
    echo ${excl_4N4B_nc}
'

 test_expect_success 'sched-params: getting params (no args) at runtime works' '
    adjust_session_info 4 &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched  sched-once=true sched-params=queue-depth=1024,delay-sched=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    flux wreck sched-params get |sort > output &&
    printf "delay-sched=true\nqueue-depth=1024\n" |sort > expected &&
    test_cmp output expected &&  
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
 test_expect_success 'sched-params: getting queue-depth at runtime works' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched  sched-once=true sched-params=queue-depth=1024,delay-sched=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    flux wreck sched-params get queue-depth > output &&
    printf "queue-depth=1024\n" > expected &&
    test_cmp output expected &&  
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
' 
 test_expect_success 'sched-params: getting delay-sched at runtime works' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched  sched-once=true sched-params=queue-depth=1024,delay-sched=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    flux wreck sched-params get delay-sched > output &&
    printf "delay-sched=true\n" > expected &&
    test_cmp output expected &&  
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
test_expect_success 'sched-params: getting both params at runtime works' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=1024,delay-sched=false &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    flux wreck sched-params get queue-depth,delay-sched |sort  > output &&
    printf "delay-sched=false\nqueue-depth=1024\n" |sort > expected &&
    test_cmp output expected &&  
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
 '
test_expect_success 'sched-params: update/get works with a short queue depth (4)' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=1024 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    test `flux wreck sched-params get queue-depth | cut -d "=" -f 2` -eq 1024 &&
    flux wreck sched-params set queue-depth=4 &&
    test `flux wreck sched-params get queue-depth | cut -d "=" -f 2` -eq 4 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
 test_expect_success 'sched-params: update/get works with another queue-depth (16)' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=4 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    test `flux wreck sched-params get queue-depth | cut -d "=" -f 2` -eq 4 &&
    flux wreck sched-params set queue-depth=16 &&
    test `flux wreck sched-params get queue-depth | cut -d "=" -f 2` -eq 16 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
 '
test_expect_success 'sched-params: update/get works with a long queue depth (4096)' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=queue-depth=2048 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    test `flux wreck sched-params get queue-depth | cut -d "=" -f 2` -eq 2048 &&
    flux wreck sched-params set queue-depth=4096 &&
    test `flux wreck sched-params get queue-depth | cut -d "=" -f 2` -eq 4096 &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
test_expect_success 'sched-params: update/get works with several delay_sched changes' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=delay-sched=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    test `flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    flux wreck sched-params set delay-sched=false &&
    test !`flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    flux wreck sched-params set delay-sched=true &&
    test `flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    flux wreck sched-params set delay-sched=false &&
    test !`flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    flux wreck sched-params set delay-sched=true &&
    test !`flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
test_expect_success 'sched-params: update/get works with delay-sched=true' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true sched-params=delay-sched=true &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    test `flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    flux wreck sched-params set delay-sched=true &&
    test `flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
test_expect_success 'sched-params: update to delay can be combined with a short depth' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    printf  "delay-sched=true\nqueue-depth=16\n" |sort > expected1 &&
    printf "delay-sched=true\nqueue-depth=32\n"  |sort > expected2 &&
    flux module load sched sched-once=true \
    sched-params=delay-sched=true,queue-depth=16 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    flux wreck sched-params get |sort > output1 &&
    flux wreck sched-params set queue-depth=32,delay-sched=true &&
    flux wreck sched-params get |sort > output2 &&
    test_cmp output1 expected1 &&  
    test_cmp output2 expected2 &&  
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
test_expect_success 'sched-params: update to delay can be made individually' '
    adjust_session_info 4 &&
    flux module remove sched &&
    flux hwloc reload ${excl_4N4B} &&
    flux module load sched sched-once=true \
    sched-params=queue-depth=2048 &&
    timed_wait_job 5 &&
    submit_1N_nproc_sleep_jobs ${excl_4N4B_nc} 0 &&
    flux wreck sched-params set delay-sched=false &&
    test `flux wreck sched-params get delay-sched | cut -d "=" -f 2` &&
    timed_sync_wait_job 10 &&
    verify_1N_nproc_sleep_jobs ${excl_4N4B_nc} 
'
test_expect_success 'sched-params: unloaded sched module' '
    flux module remove sched
'
test_done
