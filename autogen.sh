#!/bin/sh

mkdir -p m4
autoreconf -vfi
intltoolize -c -f

./configure "$@"
