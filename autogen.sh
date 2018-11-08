#!/bin/sh
echo "Running libtoolize --automake --copy ... "
libtoolize --automake --copy || exit
echo "Running autoreconf --verbose --install"
autoreconf --verbose --install || exit
echo "Moving aclocal.m4 to config/ ..."
mv aclocal.m4 config/ || exit
echo "Cleaning up ..."
rm -rf autom4te.cache
echo "Now run ./configure."
