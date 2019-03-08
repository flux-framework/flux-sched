TESTS = run_sanity_check.sh

check_SCRIPTS = $(TESTS)

CLEANFILES = $(check_SCRIPTS)
EXTRA_DIST = resource-bench.sh benchmark.graphml.in benchmark.yaml run_sanity_check.sh.in

do_subst = \
    sed -e 's,@SRC_PATH@,$(top_srcdir)/resource/utilities/test,g'

run_sanity_check.sh : run_sanity_check.sh.in
	$(do_subst) < $< > $@
	chmod +x $@

