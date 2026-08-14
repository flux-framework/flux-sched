#!/bin/sh

test_description='test rank-less (rabbit ssd) partial cancel does not leak'

. $(dirname $0)/sharness.sh

cmd_dir="${SHARNESS_TEST_SRCDIR}/data/resource/commands/rabbit_cancel"
exp_dir="${SHARNESS_TEST_SRCDIR}/data/resource/expected/rabbit_cancel"
rabbit_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit.json"
rabbit_nested_jgf="${SHARNESS_TEST_SRCDIR}/data/resource/jgfs/rabbit-nested.json"
query="../../resource/utilities/resource-query"

#
# Background: vertices that are never named in a freed R fragment --
# rank-less rabbit ssd vertices, and ranked vertices such as rabbits whose
# rank never appears in a .free (only compute node ranks do) -- used to
# leak their per-job allocation/tag state when a job holding them was
# released via an rv1exec partial-cancel followed by a full cancel.
# Job removal now releases every vertex holding state keyed by the
# retiring job, regardless of whether a freed R fragment or a tag trail
# could reach it; exclusive-filter spans are also no longer removed
# while the job still holds resources.  Every test below allocates,
# releases and then verifies with "find" that no vertex is left tagged or
# allocated to the released jobid(s), and that the freed capacity can be
# re-allocated.
#

#
# Case (a): allocate, partial-cancel the job's two node ranks in a single
# rv1exec fragment, then fully cancel the job.  Run under four match
# policies that all select the same chassis1 nodes (hetchy1017/hetchy1018)
# for this jobspec: first (the default), high, hinodex and firstnodex.
#

cmds001="${cmd_dir}/cmds01.in"
test001_desc="partial-cancel + cancel of rabbit ssd job does not leak (pol=first)"
test_expect_success "${test001_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds001} > cmds001 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P first -t 001.R.out < cmds001 &&
    test_cmp 001.R.out ${exp_dir}/001.R.out
'

cmds002="${cmd_dir}/cmds01.in"
test002_desc="partial-cancel + cancel of rabbit ssd job does not leak (pol=high)"
test_expect_success "${test002_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds002} > cmds002 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P high -t 002.R.out < cmds002 &&
    test_cmp 002.R.out ${exp_dir}/002.R.out
'

cmds003="${cmd_dir}/cmds01.in"
test003_desc="partial-cancel + cancel of rabbit ssd job does not leak (pol=hinodex)"
test_expect_success "${test003_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds003} > cmds003 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P hinodex -t 003.R.out < cmds003 &&
    test_cmp 003.R.out ${exp_dir}/003.R.out
'

cmds004="${cmd_dir}/cmds01.in"
test004_desc="partial-cancel + cancel of rabbit ssd job does not leak (pol=firstnodex)"
test_expect_success "${test004_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds004} > cmds004 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P firstnodex -t 004.R.out < cmds004 &&
    test_cmp 004.R.out ${exp_dir}/004.R.out
'

# Same as case (a), but with all pruning filters set (ALL:core,ALL:node,
# ALL:ssd) so the ssd resource is also agfilter-tracked at every high-level
# vertex; the sweep must still release the ssd's jobid state.

cmds005="${cmd_dir}/cmds01.in"
test005_desc="partial-cancel + cancel of rabbit ssd job does not leak (all prune filters incl. ssd)"
test_expect_success "${test005_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds005} > cmds005 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P first --prune-filters=ALL:core,ALL:node,ALL:ssd -t 005.R.out < cmds005 &&
    test_cmp 005.R.out ${exp_dir}/005.R.out
'

#
# Case (b): same as (a), but the two node ranks are partial-cancelled in
# two separate rv1exec fragments (rank 17, then rank 18) before the job is
# fully cancelled.
#

cmds006="${cmd_dir}/cmds02.in"
test006_desc="multi-fragment partial-cancel + cancel of rabbit ssd job does not leak"
test_expect_success "${test006_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds006} > cmds006 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P first -t 006.R.out < cmds006 &&
    test_cmp 006.R.out ${exp_dir}/006.R.out
'

#
# Case (c): ratchet/outage coverage.  Two jobs each take one chassis'
# worth of rabbit ssd capacity; both are released (partial-cancel then
# cancel).  Before the fix, the entire rabbit capacity of both chassis
# would be fossilized and neither of two subsequent jobs could allocate.
#

cmds007="${cmd_dir}/cmds03.in"
test007_desc="releasing two jobs across both chassis does not fossilize rabbit capacity"
test_expect_success "${test007_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds007} > cmds007 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P first -t 007.R.out < cmds007 &&
    test_cmp 007.R.out ${exp_dir}/007.R.out
'

#
# Case (d): same flow as (a), but the ssd share is requested without
# "exclusive: true" (non-exclusive/tagged ssd allocation rather than a
# fully exclusive one).
#

cmds008="${cmd_dir}/cmds04.in"
test008_desc="partial-cancel + cancel of non-exclusive rabbit ssd job does not leak"
test_expect_success "${test008_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds008} > cmds008 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P first -t 008.R.out < cmds008 &&
    test_cmp 008.R.out ${exp_dir}/008.R.out
'

#
# Case (e): a single rv1exec partial-cancel names *all* of the ranks the
# job holds (both hetchy1017 and hetchy1018) in one fragment, with no
# separate "cancel" call.  Note this does *not* cause an automatic full
# retirement of the job: full_cancel only becomes true when the partial
# cancel's own resource accounting equals the job's entire allocation, and
# the rank-less ssd share (which cannot be named in an R_lite fragment)
# always remains outstanding, so the job stays ALLOCATED and its ssd stays
# tagged/allocated.  This was verified both with the default prune filters
# and with --prune-filters=ALL:core,ALL:node,ALL:ssd -- neither causes
# full_cancel to fire from the partial-cancel alone. An explicit "cancel"
# is therefore still required to retire the job and release its rank-less
# ssd vertices, which is what this test checks.
#

cmds009="${cmd_dir}/cmds05.in"
test009_desc="partial-cancel naming all ranks needs an explicit cancel to fully release rabbit ssd"
test_expect_success "${test009_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds009} > cmds009 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P first -t 009.R.out < cmds009 &&
    test_cmp 009.R.out ${exp_dir}/009.R.out
'

#
# Case (f): nested topology (chassis -> rabbit(ranked) -> ssd(rank-less))
# instead of rabbit.json's flat (chassis -> ssd(rank-less)) layout.  The
# rabbit vertex itself carries a rank (19/20), but that rank is never
# named in a production .free fragment, so like the rank-less ssds below
# it the rabbit's job state can only be released by the retirement sweep.
# The fragment here deliberately names only the compute node ranks, as
# flux-core housekeeping would.
#

cmds010="${cmd_dir}/cmds06.in"
test010_desc="partial-cancel + cancel does not leak rabbit ssd in nested chassis/rabbit/ssd topology"
test_expect_success "${test010_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds010} > cmds010 &&
    ${query} -f jgf -L ${rabbit_nested_jgf} -S CA -P first -t 010.R.out < cmds010 &&
    test_cmp 010.R.out ${exp_dir}/010.R.out
'

#
# Case (g): double-booking window.  Job 1 takes chassis1 exclusively (both
# hetchy1017/hetchy1018 nodes plus both of chassis1's rank-less ssds, via
# -P firstnodex).  Only rank 17 is partial-cancelled (a single rv1exec
# fragment), so job 1 still holds hetchy1018 and both ssds, and -- per the
# current semantics -- chassis1 itself (their common ancestor) still holds
# job 1's exclusive-filter (x_checker) span, even though hetchy1017 itself
# is now free.  While job 1 is in this partially-released state, a second
# job requesting BOTH of the graph's two chassis exclusively (there are
# only two, so this can only succeed if every chassis is free of any other
# job's exclusive hold) must fail to match: chassis0 is free but chassis1
# is not, because it still retains job 1's ancestor x_span. Only after job
# 1 is fully cancelled does chassis1's x_span drop and the two-chassis job
# match. This protects the retained-ancestor-x_span semantics: on a
# version that stripped the ancestor x_span on the first partial-cancel
# fragment, the second job would incorrectly match chassis1 (and thus
# double-book it with job 1) during the window.
#

cmds011="${cmd_dir}/cmds07.in"
test011_desc="partially-released chassis is unavailable to an exclusive-chassis job until full cancel"
test_expect_success "${test011_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds011} > cmds011 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P firstnodex -t 011.R.out < cmds011 &&
    test_cmp 011.R.out ${exp_dir}/011.R.out
'

#
# Case (h): reconstruction.  Simulate a module reload: an allocation's JGF
# is captured (via a first, separate resource-query session using -F jgf),
# then fed into a fresh session with "update allocate jgf" to reconstruct
# job 1's state from scratch, exactly as the resource module would after
# restarting and replaying its R archive.  The reconstructed job is then
# cancelled and every find predicate must come up empty, and the freed
# capacity must be re-allocatable.  This checks that state built by the
# reader-driven reconstruction path (rather than by the traverser's own
# match/allocate) is retired completely via the by_jobid index, including
# the rank-less ssd vertices that can never be named in a rank-keyed
# fragment.
#

cmds012a="${cmd_dir}/cmds08a.in"
cmds012b="${cmd_dir}/cmds08b.in"
test012_desc="update-allocate-reconstructed rabbit ssd job is fully retired by cancel"
test_expect_success "${test012_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012a} > cmds012a &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P firstnodex -F jgf -t gen012.out < cmds012a &&
    grep -v INFO gen012.out > alloc011.json &&
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds012b} > cmds012b &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P firstnodex -t 012.R.out < cmds012b &&
    test_cmp 012.R.out ${exp_dir}/012.R.out
'

#
# Case (i): JGF negative path -- missing allocation span.  chassis1 is a
# non-exclusive containment vertex, so after job 1 allocates through it,
# chassis1 carries job 1's tag and ancestor x_checker span but (because
# upd_plan never records a planner allocation on a non-exclusive vertex)
# no allocation span.  A JGF partial-cancel fragment that names only
# chassis1 therefore hits the reader's "can't find allocation span" path
# and the command must fail with the documented error on stderr, without
# leaving the graph corrupted: chassis1's tag/x_span already dropped by the
# failed attempt is still reachable via job 1's by_jobid index, and a
# subsequent full "cancel 1" must retire it (and everything else job 1
# still holds) cleanly, with every find predicate coming up empty
# afterwards and the freed capacity re-allocatable.
#

cmds013="${cmd_dir}/cmds09.in"
test013_desc="partial-cancel JGF fragment missing an allocation span fails cleanly and full cancel still retires the job"
test_expect_success "${test013_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds013} > cmds013 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P firstnodex -t 013.R.out < cmds013 2>013.R.err &&
    test_cmp 013.R.out ${exp_dir}/013.R.out &&
    test_cmp 013.R.err ${exp_dir}/013.R.err
'

#
# Case (j): shrink with a live job.  While job 1 still holds hetchy1017,
# hetchy1018 and both of chassis1's ssds, rank 17 is physically removed
# from the graph via "remove 17 false" (simulating a node permanently
# leaving the cluster, as opposed to a job releasing it).  This both drops
# job 1's state on that rank and deletes the vertex outright, shrinking
# the graph out from under the still-live job.  A subsequent full
# "cancel 1" must still complete without error and retire everything job 1
# still holds (hetchy1018 and both ssds), leaving every find predicate
# empty, and the shrunken graph must still be able to satisfy a fresh
# allocation of the same shape.
#

cmds014="${cmd_dir}/cmds10.in"
test014_desc="cancel of a live job completes cleanly after one of its ranks is physically removed from the graph"
test_expect_success "${test014_desc}" '
    sed "s~@TEST_SRCDIR@~${SHARNESS_TEST_SRCDIR}~g" ${cmds014} > cmds014 &&
    ${query} -f jgf -L ${rabbit_jgf} -S CA -P firstnodex -t 014.R.out < cmds014 2>014.R.err &&
    test_cmp 014.R.out ${exp_dir}/014.R.out &&
    test_cmp 014.R.err ${exp_dir}/014.R.err
'

test_done
