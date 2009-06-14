#!/bin/sh

mkdir -p build/cache
intltoolize -c -f
autoreconf -vfi

./configure "$@"
