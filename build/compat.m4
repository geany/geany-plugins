dnl taken from Autoconf's m4sh.m4, GPLv3+
m4_ifndef([AS_VAR_COPY], [
m4_define([AS_VAR_COPY],
[AS_LITERAL_WORD_IF([$1[]$2], [$1=$$2], [eval $1=\$$2])])
])

dnl taken from pkg-config's pkg.m4, GPLv2+
m4_ifndef([PKG_CHECK_VAR], [
AC_DEFUN([PKG_CHECK_VAR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl

_PKG_CONFIG([$1], [variable="][$3]["], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])

AS_VAR_IF([$1], [""], [$5], [$4])dnl
])dnl PKG_CHECK_VAR
])
