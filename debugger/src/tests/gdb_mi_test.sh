#!/bin/sh

set -e

srcdir=${srcdir:-.}

strip_comments()
{
  ${SED:-sed} -e '/^#/d'
}

TMPOUT=tests/gdb_mi_test.output.tmp
TMPEXCPT=tests/gdb_mi_test.expected.tmp

trap 'rm -f "$TMPOUT" "$TMPEXCPT"' EXIT QUIT TERM INT

strip_comments < "$srcdir/tests/gdb_mi_test.input" | ./gdb_mi_test 2> "$TMPOUT"
strip_comments < "$srcdir/tests/gdb_mi_test.expected" > "$TMPEXCPT"
diff -u "$TMPEXCPT" "$TMPOUT"
