ACLOCAL_AMFLAGS = -I config

SUBDIRS = src rdl resrc resource simulator sched etc t

EXTRA_DIST= \
	conf \
	config/tap-driver.sh \
	DISCLAIMER.LLNS \
	README.md

CODE_COVERAGE_IGNORE_PATTERN = \
    "$(abs_top_builddir)/t/*" \
    "*/test/*.c" \
    "*/tests/*.c" \
    "*/common/libtap/*" \
    "*/common/liblsd/*" \
    "/usr/include/*" \
    "/usr/lib/*"
CODE_COVERAGE_LCOV_OPTIONS =
@CODE_COVERAGE_RULES@
