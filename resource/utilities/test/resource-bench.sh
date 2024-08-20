#!/bin/bash
set -x

NJOBS=4096
NNODES=256
NCORES=32
JOBSTREAM_FN="jobstream.txt"
PERF_FN="perf.out"
SCHED_FN="sched.out"
JOBSPEC_FN="benchmark.yaml"
GRUG_IN_FN="benchmark.graphml.in"
GRUG="benchmark.graphml"
SRC_PATH="./"
KEEP_FILES="false"

#
declare -r prog=${0##*/}
die() { echo -e "$prog: $@"; exit 1; }

#
declare -r long_opts="help,nnodes:,ncores:,njobs:,grug:,jobspec-fn:,path:,keep"
declare -r short_opts="hn:c:j:g:s:p:k"
declare -r usage="
Usage: $prog [OPTIONS] -- [CONFIGURE_ARGS...]\n\
Run the performance benchmark for resource-query by matching\n\
a configurable number of 1-core jobs on a simple configurable cluster.\n\
\n\
Options:\n\
 -h, --help                    Display this message\n\
 -n, --nnodes                  Num of nodes in cluster (default=${NNODES})\n\
 -c, --ncores                  Num of cores per node (default=${NCORES})\n\
 -j, --njobs                   Num of jobs to schedule (default=${NJOBS})\n\
 -g, --grug                    System GRUG input file (default=${GRUG_IN_FN})\n\
 -s, --jobspec-fn              Jobspec file name (default=${JOBSPEC_FN})\n\
 -p, --path                    Where inputs reside (default=${SRC_PATH})\n\
 -k, --keep                    Keep intermediate files\n\
"

GETOPTS=`/usr/bin/env getopt -u -o ${short_opts} -l ${long_opts} -n ${prog} -- ${@}`
if [[ $? != 0 ]]; then
    die "${usage}"
fi
eval set -- "${GETOPTS}"

while true; do
    case "${1}" in
      -h|--help)                   echo -ne "${usage}";          exit 0  ;;
      -n|--nnodes)                 NNODES="${2}";                shift 2 ;;
      -c|--ncores)                 NCORES="${2}";                shift 2 ;;
      -j|--njobs)                  NJOBS="${2}";                 shift 2 ;;
      -g|--grug)                   GRUG_IN_FN="${2}";            shift 2 ;;
      -s|--jobspec-fn)             JOBSPEC_FN="${2}";            shift 2 ;;
      -p|--path)                   SRC_PATH="${2}";              shift 2 ;;
      -k|--keep)                   KEEP_FILES="true";            shift 1 ;;
      --)                          shift; break;                         ;;
      *)                           die "Invalid option '${1}'\n${usage}" ;;
    esac
done

if [[ ! -f ${SRC_PATH}/${GRUG_IN_FN} ]]
then
    die "can't find cluster GRUG input file (${GRUG_IN_FN})!"
fi
if [[ ! -f ${SRC_PATH}/${JOBSPEC_FN} ]]
then
    die "can't find input jobspec file (${JOBSPEC_FN})!"
fi
if [[ ! -f ../resource-query ]]
then
    die "can't find resource-query!"
fi

rm -f ${GRUG} ${JOBSTREAM_FN} ${PERF_FN} ${SCHED_FN}

for job in `seq 1 ${NJOBS}`
do
    echo "match allocate ${SRC_PATH}/${JOBSPEC_FN}" >> ${JOBSTREAM_FN}
done
echo "stat" >> ${JOBSTREAM_FN}
echo "quit" >> ${JOBSTREAM_FN}

cat ${SRC_PATH}/${GRUG_IN_FN} | sed -e "s/@NNODES@/${NNODES}/" \
	                            -e "s/@NCORES@/${NCORES}/" > ${GRUG}

../resource-query -L ${GRUG} -e -d -t ${SCHED_FN} < ${JOBSTREAM_FN} > ${PERF_FN}

if test $? -ne 0 || test ! -f ${SCHED_FN} || test ! -f ${PERF_FN}
then
    die "errors in resource-query!"
fi

cat ${PERF_FN} | sed 's/INFO: //'

if [[ ${KEEP_FILES} = "false" ]]
then
    rm -f ${GRUG} ${JOBSTREAM_FN} ${PERF_FN} ${SCHED_FN}
fi

