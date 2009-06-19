#!/bin/bash

# Script to clean source directory down to bare-bones...

rm -rfv '.deps' '.libs' '_libs' 'autom4te.cache' 'configure' 'aclocal.m4' \
'libtool' 'ltmain.sh' 'Makefile.in' 'Makefile' 'depcomp' 'install-sh' 'missing' \
*.tar.gz    config.*  stamp-h* $(find -type l) \
$(find -type f \( -name '*.so*' -or -name '*.dll' -or -name '*.[ao]' -or -name '*.l[ao]' \) )



