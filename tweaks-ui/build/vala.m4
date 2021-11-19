dnl GP_CHECK_VALAC([version], [action-if-found], [action-if-not-found])
dnl Checks for the Vala compiler.  Similar to AM_PROG_VALAC but supports
dnl 2nd and 3rd arguments with Automake < 1.12.5
AC_DEFUN([GP_CHECK_VALAC],
[
    gp_have_valac=yes
    AM_PROG_VALAC([$1],,[gp_have_valac=no])
    dnl Automake < 1.12.5 does not support 3rd arg but leaves $VALAC
    dnl empty if not found, so emulate it
    AS_IF([test "x$VALAC" = x], [gp_have_valac=no])
    AS_IF([test "$gp_have_valac" = yes],
          [m4_default([$2], [:])],
          [m4_default([$3], [:])])
])

dnl GP_CHECK_PLUGIN_VALA(plugin, [valac-version])
dnl Checks for the Vala compiler and takes the appropriate action on
dnl $plugin depending on its presence.
dnl FIXME: take into account that the compiler is optional if we have
dnl        the generated C sources
AC_DEFUN([GP_CHECK_PLUGIN_VALA],
[
    AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = no],[],
          [GP_CHECK_VALAC([$2],
                          [dnl we fake the checking messages here because
                           dnl GP_CHECK_VALAC outputs messages itself
                           AC_MSG_CHECKING([whether the vala compiler is compatible with $1])
                           AC_MSG_RESULT([yes])],
                          [AC_MSG_CHECKING([whether the vala compiler is compatible with $1])
                           AC_MSG_RESULT([no])
                           AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = yes],
                                 [AC_MSG_ERROR([$1 requires the Vala compiler])],
                                 [test "$m4_tolower(AS_TR_SH(enable_$1))" = auto],
                                 [m4_tolower(AS_TR_SH(enable_$1))=no])])])
])
