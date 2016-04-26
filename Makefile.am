
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src rdl resrc simulator sched etc t

EXTRA_DIST= \
	conf \
	m4/tap-driver.sh \
	DISCLAIMER.LLNS \
	README.md

CODE_COVERAGE_IGNORE_PATTERN = \
    "$(abs_top_builddir)/t/*" \
    "/test/*.c" \
    "/tests/*.c" \
    "common/libtap/*" \
    "common/liblsd/*" \
    "/usr/*"
CODE_COVERAGE_LCOV_OPTIONS =
@CODE_COVERAGE_RULES@
