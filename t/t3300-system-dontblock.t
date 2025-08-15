#!/bin/sh
#

test_description='Test fluxion does not block on R w/ no scheduling key
'
. `dirname $0`/sharness.sh

export TEST_UNDER_FLUX_QUORUM=1
export TEST_UNDER_FLUX_START_MODE=leader
export FLUX_RC_EXTRA=${SHARNESS_TEST_SRCDIR}/../etc
unset FLUXION_RESOURCE_RC_NOOP
unset FLUXION_QMANAGER_RC_NOOP
export FLUXION_RESOURCE_OPTIONS="load-allowlist=node,core,gpu load-format=hwloc"

if test -n "$FLUX_RC_USE_MODPROBE"; then
    unset FLUX_MODPROBE_DISABLE
    # modprobe is in use, and therefore failure to load Fluxion modules
    # causes instance failure instead of falling back to sched-simple.
    # Therefore test with flux-start(1) instead of test_under_flux() as
    # below.
    test_expect_success 'fluxion immediately fails to be loaded with hwloc (modprobe)' '
	test_must_fail flux start -Slog-stderr-level=7 \
		-Sbroker.module-nopanic=1 \
		flux module stats sched-fluxion-resource \
		>modprobe.out 2>&1 &&
	test_debug "cat modprobe.out" &&
	grep fluxion modprobe.out
    '
else

    # Comment in the following to generated scheduling key to R
    # TEST_UNDER_FLUX_AUGMENT_R=t

    test_under_flux 2 system -Sbroker.module-nopanic=1

    SCHED_MODULE=$(flux module list | awk '$NF == "sched" {print $1}')

    test_expect_success 'fluxion immediately fails to be loaded with hwloc reader' '
        test_debug "echo sched service provided by ${SCHED_MODULE}" &&
        echo $SCHED_MODULE > out &&
        test "$SCHED_MODULE" != "sched-fluxion-qmanager"
    '
fi

test_done
