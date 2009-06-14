#!/bin/sh

mkdir -p build/m4/cache
autoreconf -vfi
intltoolize -c -f

./configure "$@"
