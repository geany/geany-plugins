#!/bin/sh

set -e

srcdir=${srcdir:-.}

strip_comments()
{
  ${SED:-sed} -e '/^#/d'
}

SUBDIR=tests
TMPOUT=$SUBDIR/gdb_mi_test.output.tmp
TMPEXCPT=$SUBDIR/gdb_mi_test.expected.tmp

trap 'rm -f "$TMPOUT" "$TMPEXCPT";
      rmdir "$SUBDIR" 2>/dev/null || :' EXIT QUIT TERM INT

test -d "$SUBDIR" || mkdir "$SUBDIR"
strip_comments < "$srcdir/tests/gdb_mi_test.input" | ./gdb_mi_test 2> "$TMPOUT"
strip_comments < "$srcdir/tests/gdb_mi_test.expected" > "$TMPEXCPT"
diff -u "$TMPEXCPT" "$TMPOUT"
