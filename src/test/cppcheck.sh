#!/bin/sh
cppcheck --force --inline-suppr -j 2 --std=c99 --quiet \
    --error-exitcode=1 \
    -i src/common/libtap \
    src
