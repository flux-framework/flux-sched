#!/bin/sh

test_description='Test that parent duration is inherited according to RFC14'

. `dirname $0`/sharness.sh

#
# test_under_flux is under sharness.d/
#
test_under_flux 1

test_expect_success HAVE_JQ 'parent duration is inherited when duration=0' '
	cat >get_R.sh <<-EOT &&
	#!/bin/sh

	flux job info \$FLUX_JOB_ID R
	EOT
	chmod +x get_R.sh &&
	out=$(flux mini run -t20s -n1 flux start flux mini run -n1 ./get_R.sh) &&
	echo "$out" | jq -e ".execution.expiration - .execution.starttime <= 20" &&
	out=$(flux mini run -t30s -n1 flux start flux mini run -n1 ./get_R.sh) &&
	echo "$out" | jq -e ".execution.expiration - .execution.starttime <= 30"
'

test_done
