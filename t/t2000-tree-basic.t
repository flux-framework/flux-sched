#!/bin/sh

test_description='Test flux-tree correctness'

. `dirname $0`/sharness.sh

if test -z "${FLUX_SCHED_TEST_INSTALLED}" || test -z "${FLUX_SCHED_CO_INST}"
 then
     export FLUX_RC_EXTRA="${SHARNESS_TEST_SRCDIR}/../etc"
fi

remove_prefix(){
    awk '{for (i=3; i<=NF; i++) printf $i " "; print ""}' ${1} > ${2}
}

test_expect_success 'flux-tree: --dry-run works' '
    flux tree --dry-run -N 1 -c 1 /bin/hostname
'

test_expect_success 'flux-tree: relative jobscript command path works' '
    flux tree --dry-run -N 1 -c 1 hostname
'

test_expect_success 'flux-tree: --leaf works' '
    cat >cmp.01 <<-EOF &&
	FLUX_TREE_ID=tree
	FLUX_TREE_JOBSCRIPT_INDEX=1
	FLUX_TREE_NCORES_PER_NODE=1
	FLUX_TREE_NGPUS_PER_NODE=0
	FLUX_TREE_NNODES=1
	eval /bin/hostname
EOF
    flux tree --dry-run --leaf -N 1 -c 1 /bin/hostname > out.01 &&
    remove_prefix out.01 out.01.a &&
    sed -i "s/[ \t]*$//g" out.01.a &&
    test_cmp cmp.01 out.01.a
'

test_expect_success 'flux-tree: -- option works' '
    cat >cmp.01.1 <<-EOF &&
	FLUX_TREE_ID=tree
	FLUX_TREE_JOBSCRIPT_INDEX=1
	FLUX_TREE_NCORES_PER_NODE=1
	FLUX_TREE_NGPUS_PER_NODE=0
	FLUX_TREE_NNODES=1
	eval hostname -h
EOF
    flux tree --dry-run --leaf -N 1 -c 1 -- hostname -h > out.01.1 &&
    remove_prefix out.01.1 out.01.1.a &&
    sed -i "s/[ \t]*$//g" out.01.1.a &&
    test_cmp cmp.01.1 out.01.1.a
'

test_expect_success 'flux-tree: multi cmdline works' '
    cat >cmp.01.2 <<-EOF &&
	FLUX_TREE_ID=tree
	FLUX_TREE_JOBSCRIPT_INDEX=1
	FLUX_TREE_NCORES_PER_NODE=1
	FLUX_TREE_NGPUS_PER_NODE=0
	FLUX_TREE_NNODES=1
	eval flux python hostname.py
EOF
    flux tree --dry-run --leaf -N 1 -c 1 flux python hostname.py > out.01.2 &&
    remove_prefix out.01.2 out.01.2.a &&
    sed -i "s/[ \t]*$//g" out.01.2.a &&
    test_cmp cmp.01.2 out.01.2.a
'

test_expect_success 'flux-tree: nonexistent script can be detected' '
    test_must_fail flux tree --dry-run -N 1 -c 1 ./nonexistent
'

test_expect_success 'flux-tree: --njobs works' '
    cat >cmp.02 <<-EOF &&
	FLUX_TREE_ID=tree
	FLUX_TREE_JOBSCRIPT_INDEX=1
	FLUX_TREE_NCORES_PER_NODE=1
	FLUX_TREE_NGPUS_PER_NODE=0
	FLUX_TREE_NNODES=1
	eval /bin/hostname
	FLUX_TREE_ID=tree
	FLUX_TREE_JOBSCRIPT_INDEX=2
	FLUX_TREE_NCORES_PER_NODE=1
	FLUX_TREE_NGPUS_PER_NODE=0
	FLUX_TREE_NNODES=1
	eval /bin/hostname
EOF
    flux tree --dry-run --leaf -N 1 -c 1 -J 2 /bin/hostname > out.02 &&
    remove_prefix out.02 out.02.2 &&
    sed -i "s/[ \t]*$//g" out.02.2 &&
    test_cmp cmp.02 out.02.2
'

test_expect_success 'flux-tree: --prefix to detect level != toplevel works' '
    cat >cmp.03 <<-EOF &&
	FLUX_TREE_ID=tree.1
	FLUX_TREE_JOBSCRIPT_INDEX=1
	FLUX_TREE_NCORES_PER_NODE=1
	FLUX_TREE_NGPUS_PER_NODE=0
	FLUX_TREE_NNODES=1
	eval /bin/hostname
EOF
    flux tree --dry-run --leaf -N 1 -c 1 -X tree.1 /bin/hostname > out.03 &&
    remove_prefix out.03 out.03.2 &&
    sed -i "s/[ \t]*$//g" out.03.2 &&
    test_cmp cmp.03 out.03.2
'

test_expect_success 'flux-tree: --topology and --leaf cannot be given together' '
    test_must_fail flux tree --dry-run -T 2 -l -N 1 -c 1 /bin/hostname
'

# cmp.04 contains parameters to use to spawn a child instance
test_expect_success 'flux-tree: --topology=1 works' '
    cat >cmp.04 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=1
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=1 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=1 /bin/hostname > out.04 &&
    remove_prefix out.04 out.04.2 &&
    sed -i "s/[ \t]*$//g" out.04.2 &&
    test_cmp cmp.04 out.04.2
'

test_expect_success 'flux-tree: -T2 on 1 node/1 core is equal to -T1' '
    cat >cmp.05 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=1
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=1 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=2 /bin/hostname > out.05 &&
    remove_prefix out.05 out.05.2 &&
    sed -i "s/[ \t]*$//g" out.05.2 &&
    test_cmp cmp.05 out.05.2
'

test_expect_success 'flux-tree: -T3 on 1 node/1 core is equal to -T1' '
    cat >cmp.06 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=1
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=1 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=3 /bin/hostname > out.06 &&
    remove_prefix out.06 out.06.2 &&
    sed -i "s/[ \t]*$//g" out.06.2 &&
    test_cmp cmp.06 out.06.2
'

test_expect_success 'flux-tree: -T1 on 2 nodes/2 cores work' '
    cat >cmp.07 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=2 c=2
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=1 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=1 -N 2 -c 2 /bin/hostname > out.07 &&
    remove_prefix out.07 out.07.2 &&
    sed -i "s/[ \t]*$//g" out.07.2 &&
    test_cmp cmp.07 out.07.2
'

test_expect_success 'flux-tree: -T2 on 2 nodes/2 cores work' '
    cat >cmp.08 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=2
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=5 S=/bin/hostname
	
	Rank=2: N=1 c=2
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=5 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=2 -N 2 -c 2 -J 10 /bin/hostname > out.08 &&
    remove_prefix out.08 out.08.2 &&
    sed -i "s/[ \t]*$//g" out.08.2 &&
    test_cmp cmp.08 out.08.2
'

# Not all instance will run simultaneously
test_expect_success 'flux-tree: -T3 on 4 nodes/4 cores work' '
    cat >cmp.09 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=2 c=3
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=4 S=/bin/hostname
	
	Rank=2: N=2 c=3
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=2 c=3
	Rank=3: T=--leaf
	Rank=3:
	Rank=3: X=--prefix=tree.3 J=--njobs=3 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=3 -N 4 -c 4 -J 10 /bin/hostname > out.09 &&
    remove_prefix out.09 out.09.2 &&
    sed -i "s/[ \t]*$//g" out.09.2 &&
    test_cmp cmp.09 out.09.2
'

test_expect_success 'flux-tree: -T4 on 4 nodes/4 cores work' '
    cat >cmp.10 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=4
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4
	Rank=3: T=--leaf
	Rank=3:
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4
	Rank=4: T=--leaf
	Rank=4:
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=4 -N 4 -c 4 -J 10 /bin/hostname > out.10 &&
    remove_prefix out.10 out.10.2 &&
    sed -i "s/[ \t]*$//g" out.10.2 &&
    test_cmp cmp.10 out.10.2
'

test_expect_success 'flux-tree: --perf-out generates a perf output file' '
    flux tree --dry-run -T 4 -N 4 -c 4 -J 10 -o p.out /bin/hostname &&
    test -f p.out &&
    test $(wc -l <p.out) -eq 2 &&
    test $(wc -w <p.out) -eq 18
'

test_expect_success 'flux-tree: --perf-format works with custom format' '
    flux tree --dry-run -T 2 -N 2 -c 4 -J 2 -o perf.out \
         --perf-format="{treeid}\ {elapse:f}\ {my_nodes:d}" \
         /bin/hostname &&
    test -f perf.out &&
    test $(wc -l <perf.out) -eq 2 &&
    test $(wc -w <perf.out) -eq 6
'

test_expect_success 'flux-tree: -T4x2 on 4 nodes/4 cores work' '
    cat >cmp.11 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=4
	Rank=1: T=--topology=2
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4
	Rank=2: T=--topology=2
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4
	Rank=3: T=--topology=2
	Rank=3:
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4
	Rank=4: T=--topology=2
	Rank=4:
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run -T 4x2 -N 4 -c 4 -J 10 /bin/hostname > out.11 &&
    remove_prefix out.11 out.11.2 &&
    sed -i "s/[ \t]*$//g" out.11.2 &&
    test_cmp cmp.11 out.11.2
'

test_expect_success 'flux-tree: -T4x2 -Q fcfs:easy works' '
    cat >cmp.12 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:queue-policy=fcfs
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=4
	Rank=1: T=--topology=2
	Rank=1: Q=--queue-policy=easy
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4
	Rank=2: T=--topology=2
	Rank=2: Q=--queue-policy=easy
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4
	Rank=3: T=--topology=2
	Rank=3: Q=--queue-policy=easy
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4
	Rank=4: T=--topology=2
	Rank=4: Q=--queue-policy=easy
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run -T 4x2 -Q fcfs:easy -N 4 -c 4 -J 10 /bin/hostname \
> out.12 &&
    remove_prefix out.12 out.12.2 &&
    sed -i "s/[ \t]*$//g" out.12.2 &&
    test_cmp cmp.12 out.12.2
'

test_expect_success 'flux-tree: -T4x2x3 -Q fcfs:fcfs:easy works' '
    cat >cmp.13 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:queue-policy=fcfs
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=4
	Rank=1: T=--topology=2x3
	Rank=1: Q=--queue-policy=fcfs:easy
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4
	Rank=2: T=--topology=2x3
	Rank=2: Q=--queue-policy=fcfs:easy
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4
	Rank=3: T=--topology=2x3
	Rank=3: Q=--queue-policy=fcfs:easy
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4
	Rank=4: T=--topology=2x3
	Rank=4: Q=--queue-policy=fcfs:easy
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run -T 4x2x3 -Q fcfs:fcfs:easy -N 4 -c 4 -J 10 /bin/hostname \
> out.13 &&
    remove_prefix out.13 out.13.2 &&
    sed -i "s/[ \t]*$//g" out.13.2 &&
    test_cmp cmp.13 out.13.2
'

test_expect_success 'flux-tree: combining -T -Q -P works (I)' '
    cat >cmp.14 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:queue-policy=fcfs,queue-params=queue-depth=23
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=4
	Rank=1: T=--topology=2
	Rank=1: Q=--queue-policy=conservative P=--queue-params=reservation-depth=24
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4
	Rank=2: T=--topology=2
	Rank=2: Q=--queue-policy=conservative P=--queue-params=reservation-depth=24
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4
	Rank=3: T=--topology=2
	Rank=3: Q=--queue-policy=conservative P=--queue-params=reservation-depth=24
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4
	Rank=4: T=--topology=2
	Rank=4: Q=--queue-policy=conservative P=--queue-params=reservation-depth=24
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run -T 4x2 -Q fcfs:conservative \
-P queue-depth=23:reservation-depth=24 -N 4 -c 4 -J 10 /bin/hostname > out.14 &&
    remove_prefix out.14 out.14.2 &&
    sed -i "s/[ \t]*$//g" out.14.2 &&
    test_cmp cmp.14 out.14.2
'

test_expect_success 'flux-tree: combining -T -Q -P works (II)' '
    cat >cmp.15 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:queue-policy=fcfs,queue-params=queue-depth=7,reservation-depth=8
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=4
	Rank=1: T=--topology=2
	Rank=1: Q=--queue-policy=conservative P=--queue-params=queue-depth=24
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4
	Rank=2: T=--topology=2
	Rank=2: Q=--queue-policy=conservative P=--queue-params=queue-depth=24
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4
	Rank=3: T=--topology=2
	Rank=3: Q=--queue-policy=conservative P=--queue-params=queue-depth=24
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4
	Rank=4: T=--topology=2
	Rank=4: Q=--queue-policy=conservative P=--queue-params=queue-depth=24
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run -T 4x2 -Q fcfs:conservative \
-P queue-depth=7,reservation-depth=8:queue-depth=24 -N 4 -c 4 -J 10 \
/bin/hostname > out.15 &&
    remove_prefix out.15 out.15.2 &&
    sed -i "s/[ \t]*$//g" out.15.2 &&
    test_cmp cmp.15 out.15.2
'

test_expect_success 'flux-tree: combining -T -M works' '
    cat >cmp.16 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:hwloc-allowlist=node,core,gpu policy=low
	Rank=1: N=1 c=4
	Rank=1: T=--topology=2
	Rank=1: M=--match-policy=high
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4
	Rank=2: T=--topology=2
	Rank=2: M=--match-policy=high
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4
	Rank=3: T=--topology=2
	Rank=3: M=--match-policy=high
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4
	Rank=4: T=--topology=2
	Rank=4: M=--match-policy=high
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run -T 4x2 -M low:high -N 4 -c 4 -J 10 /bin/hostname \
> out.16 &&
    remove_prefix out.16 out.16.2 &&
    sed -i "s/[ \t]*$//g" out.16.2 &&
    test_cmp cmp.16 out.16.2
'

test_expect_success 'flux-tree: existing FLUXION_QMANAGER_OPTIONS is respected' '
    cat >cmp.17 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:queue-policy=conservative
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=1
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=1 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:queue-policy=easy
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    export FLUXION_QMANAGER_OPTIONS=queue-policy=easy &&
    flux tree --dry-run -T 1 -Q conservative /bin/hostname > out.17 &&
    unset FLUXION_QMANAGER_OPTIONS &&
    remove_prefix out.17 out.17.2 &&
    sed -i "s/[ \t]*$//g" out.17.2 &&
    test_cmp cmp.17 out.17.2
'

test_expect_success 'flux-tree: existing FLUXION_RESOURCE_OPTIONS is respected' '
    cat >cmp.18 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:hwloc-allowlist=node,core,gpu policy=locality
	Rank=1: N=1 c=1
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=1 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:high
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    export FLUXION_RESOURCE_OPTIONS=high &&
    flux tree --dry-run -T 1 -M locality /bin/hostname > out.18 &&
    unset FLUXION_RESOURCE_OPTIONS &&
    remove_prefix out.18 out.18.2 &&
    sed -i "s/[ \t]*$//g" out.18.2 &&
    test_cmp cmp.18 out.18.2
'

test_expect_success 'flux-tree: -T4 on 4 nodes/4 cores/4 GPUs work' '
    cat >cmp.19 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=4 g=-g 4
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=3 S=/bin/hostname
	
	Rank=2: N=1 c=4 g=-g 4
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=3 S=/bin/hostname
	
	Rank=3: N=1 c=4 g=-g 4
	Rank=3: T=--leaf
	Rank=3:
	Rank=3: X=--prefix=tree.3 J=--njobs=2 S=/bin/hostname
	
	Rank=4: N=1 c=4 g=-g 4
	Rank=4: T=--leaf
	Rank=4:
	Rank=4: X=--prefix=tree.4 J=--njobs=2 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=4 -N 4 -c 4 -g 4 -J 10 \
/bin/hostname > out.19 &&
    remove_prefix out.19 out.19.2 &&
    sed -i "s/[ \t]*$//g" out.19.2 &&
    test_cmp cmp.19 out.19.2
'

test_expect_success 'flux-tree: -T4 on 4 nodes/4 cores/2 GPUs work' '
    cat >cmp.20 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=2 c=4 g=-g 2
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=5 S=/bin/hostname
	
	Rank=2: N=2 c=4 g=-g 2
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=5 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=4 -N 4 -c 4 -g 2 -J 10 \
/bin/hostname > out.20 &&
    remove_prefix out.20 out.20.2 &&
    sed -i "s/[ \t]*$//g" out.20.2 &&
    test_cmp cmp.20 out.20.2
'

test_expect_success 'flux-tree: -T4 on 4 nodes/4 cores/2 GPUs 1 job works' '
    cat >cmp.21 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=2 c=4 g=-g 2
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=1 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=4 -N 4 -c 4 -g 2 -J 1 \
/bin/hostname > out.21 &&
    remove_prefix out.21 out.21.2 &&
    sed -i "s/[ \t]*$//g" out.21.2 &&
    test_cmp cmp.21 out.21.2
'

test_expect_success 'flux-tree: correct job count under a unbalanced tree' '
    cat >cmp.22 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=1
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=5 S=/bin/hostname
	
	Rank=2: N=1 c=1
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=5 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=3 -N 1 -c 2 -J 10 \
/bin/hostname > out.22 &&
    remove_prefix out.22 out.22.2 &&
    sed -i "s/[ \t]*$//g" out.22.2 &&
    test_cmp cmp.22 out.22.2
'

test_expect_success 'flux-tree: --flux-rundir=DIR works in dry-run' '
    cat >cmp.23 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=2 o=-o,-Srundir=DIR/tree.1.pfs
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=5 S=/bin/hostname
	
	Rank=2: N=1 c=2 o=-o,-Srundir=DIR/tree.2.pfs
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=5 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=2 -N 2 -c 2 -J 10 -r DIR \
/bin/hostname > out.23 &&
    remove_prefix out.23 out.23.2 &&
    sed -i "s/[ \t]*$//g" out.23.2 &&
    test_cmp cmp.23 out.23.2
'

test_expect_success 'flux-tree: --flux-rundir=DIR works along with -f' '
    cat >cmp.24 <<-EOF &&
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	Rank=1: N=1 c=2 o=-o,-Slog-filename=DIR2/tree.1.log -o,-Srundir=DIR/tree.1.pfs
	Rank=1: T=--leaf
	Rank=1:
	Rank=1: X=--prefix=tree.1 J=--njobs=5 S=/bin/hostname
	
	Rank=2: N=1 c=2 o=-o,-Slog-filename=DIR2/tree.2.log -o,-Srundir=DIR/tree.2.pfs
	Rank=2: T=--leaf
	Rank=2:
	Rank=2: X=--prefix=tree.2 J=--njobs=5 S=/bin/hostname
	
	FLUXION_QMANAGER_OPTIONS:
	FLUXION_RESOURCE_OPTIONS:
	FLUXION_QMANAGER_RC_NOOP:1
	FLUXION_RESOURCE_RC_NOOP:1
EOF
    flux tree --dry-run --topology=2 -N 2 -c 2 -J 10 -r DIR -f DIR2 \
/bin/hostname > out.24 &&
    remove_prefix out.24 out.24.2 &&
    sed -i "s/[ \t]*$//g" out.24.2 &&
    test_cmp cmp.24 out.24.2
'

test_done
