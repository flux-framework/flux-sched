#!/bin/sh

test_description='Test loading JGF when parent has only R_lite (no JGF)'

. `dirname $0`/sharness.sh

if ! command -v hwloc-ls >/dev/null; then
	skip_all='Skipping JGF load tests because hwloc-ls not found'
	test_done
fi

SIZE=1
test_under_flux ${SIZE}

BUILDDIR=${SHARNESS_BUILD_DIRECTORY}
export FLUX_URI_RESOLVE_LOCAL=t

test_expect_success 'amend R with JGF' '
	hwloc-ls --of xml > test.xml &&
	printf "find status=up\nquit" | \
	    ${BUILDDIR}/resource/utilities/resource-query \
	    -L test.xml -f hwloc -F jgf -S CA -P high \
	    -W cluster,node,socket,core,gpu | head -1 > test.jgf &&
	test -s test.jgf &&
	flux kvs get resource.R | \
	jq --slurpfile jgf test.jgf ".scheduling = \$jgf[0]" > test.R
'

test_expect_success 'parent instance has R_lite but no JGF' '
	flux kvs get resource.R | jq -e ".scheduling == null"
'

test_expect_success 'create modprobe script to reload test.R' '
	mkdir rc1.d &&
	cat <<-EOF >rc1.d/load-jgf.py &&
	from flux.modprobe import task
	@task("load-jgf",
	      ranks="0",
	      before=["sched-fluxion-resource"],
	      after=["resource"]
	)
	def load_jgf(context):
	    context.bash("flux resource reload test.R")
	EOF
	flux run -N1 \
	    --env=-FLUX_MODPROBE_DISABLE \
	    --env=FLUX_MODPROBE_PATH=${FLUX_MODPROBE_PATH}:$(pwd) \
	    flux dmesg -H
'

test_expect_success 'run subinstance with JGF loaded' '
	jobid=$(flux alloc -N1 --bg \
		--env=-FLUX_MODPROBE_DISABLE \
		--env=FLUX_MODPROBE_PATH=${FLUX_MODPROBE_PATH}:$(pwd))
'

test_expect_success 'JGF was loaded in subinstance' '
	flux proxy $jobid flux dmesg -H | grep sched-fluxion-resource &&
	flux proxy $jobid flux dmesg -H | grep "loaded with JGF"
'

test_expect_success 'can query resources with JGF' '
	flux proxy $jobid flux ion-resource find status=up
'

test_done
