#!/bin/sh

mkdir -p build/m4/cache
intltoolize -c -f
autoreconf -vfi

#./configure "$@"
