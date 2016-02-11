#!/bin/sh

echo "Running autoreconf -i ... "
autoreconf -i
echo "Cleaning up ..."
rm -rf autom4te.cache
echo "Now run ./configure."
