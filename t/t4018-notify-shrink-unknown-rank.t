#!/bin/sh
#
# The excluded rank 3 is not in the resource graph of sched-fluxion-resource.
# A resource.acquire shrink that names rank 3 together with a real rank
# makes the traverser ignore rank 3 with a warning, and the removal of the
# subgraph and the initialization succeed. The module must report this
# shrink to the subscribers of sched-fluxion-resource.notify, for example
# the feasibility module, and must keep it in the history of lost ranks for
# a new subscriber. Before the fix, the partial cancel failed, and the
# module did not report the shrink although the graph lost rank 2.

test_description='A shrink with a rank that the scheduler does not know is published'

. `dirname $0`/sharness.sh

conf_base=${SHARNESS_TEST_SRCDIR}/conf.d
notify_base=`readlink -e ${SHARNESS_TEST_SRCDIR}/data/resource/jobspecs/satisfiability`

SIZE=4
export FLUX_URI_RESOLVE_LOCAL=t
export FLUX_SCHED_MODULE=none

test_under_flux $SIZE full --test-exit-mode=leader \
	--config-path=${conf_base}/exclude-rank3

force_down () {
	flux python -c "import flux; flux.Flux().rpc(\"resource.monitor-force-down\", {\"ranks\":\"$1\"}).get()"
}

if ! force_down "" 2>/dev/null ; then
	skip_all='resource.monitor-force-down failed, skipping all tests'
	test_done
fi

test_expect_success 'rank 3 is excluded, so fluxion has ranks 0-2' '
	load_resource &&
	load_feasibility &&
	flux ion-resource match satisfiability ${notify_base}/shrink3.yaml &&
	test_must_fail flux ion-resource match allocate_with_satisfiability \
		${notify_base}/shrink4.yaml
'

test_expect_success 'shrink an excluded and a real rank in one update' '
	flux dmesg -C &&
	force_down "2,3" &&
	flux dmesg -H | grep "partial cancel by ranks" | grep "rank 3 is not in the by_rank map" &&
	flux dmesg -H | grep "successfully removed ranks 2-3"
'

test_expect_success 'the primary graph lost rank 2' '
	test_must_fail flux ion-resource match allocate_with_satisfiability \
		${notify_base}/shrink3.yaml
'

test_expect_success 'the subscribed feasibility module lost rank 2 too' '
	test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink3.yaml
'

test_expect_success 'a new subscriber also learns about rank 2' '
	reload_feasibility &&
	test_must_fail flux ion-resource match satisfiability ${notify_base}/shrink3.yaml
'

test_expect_success 'unload fluxion modules' '
	remove_feasibility &&
	remove_resource
'

test_done
