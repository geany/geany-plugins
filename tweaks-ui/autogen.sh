#!/bin/sh -e

mkdir -p build/cache po
#intltoolize -c -f
autoreconf -vfi

if [ "$NOCONFIGURE" = 1 ]; then
    echo "Done. configure skipped."
    exit 0;
fi
exec ./configure "$@"
