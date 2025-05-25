#!/bin/sh -e

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

mkdir -p build/cache

(cd "$srcdir"; autoreconf --force --install --verbose)

if [ "$NOCONFIGURE" = 1 ]; then
    echo "Done. configure skipped."
    exit 0;
fi

echo "Running $srcdir/configure $@ ..."
"$srcdir/configure" "$@" && echo "Now type 'make' to compile." || exit 1
