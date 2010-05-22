#!/bin/sh


mkdir -p build/aux          || exit 1
mkdir -p build/m4           || exit 1
#~ gtkdocize --flavour no-tmpl || exit 1
glib-gettextize -c -f       || exit 1
autoreconf -vfi             || exit 1
