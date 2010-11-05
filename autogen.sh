#!/bin/sh

mkdir -p build/cache
intltoolize -c -f
autoreconf -vfi

if [ "$NOCONFIGURE" = 1 ]; then
    echo "Done. configure skipped."
    exit 0;
fi
exec ./configure "$@"
