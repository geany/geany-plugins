dnl GP_CHECK_CPPCHECK
dnl Checks for cppcheck
AC_DEFUN([GP_CHECK_CPPCHECK],
[
    AC_ARG_ENABLE([cppcheck],
                  AS_HELP_STRING([--enable-cppcheck],
                                 [use cppcheck to check the source code
                                  @<:@default=auto@:>@]),
                  [enable_cppcheck="$enableval"],
                  [enable_cppcheck="auto"])

    gp_have_cppcheck=no
    AS_IF([test "x$enable_cppcheck" != xno],
          [AC_PATH_PROG([CPPCHECK], [cppcheck], [NONE])
           AS_IF([test "x$CPPCHECK" != xNONE],
                 [gp_have_cppcheck=yes
                  AC_SUBST([CPPCHECK])],
                 [gp_have_cppcheck=no
                  AS_IF([test "x$enable_cppcheck" != xauto],
                        [AC_MSG_ERROR([cannot find cppcheck])])])])
    AM_CONDITIONAL([HAVE_CPPCHECK], [test "x$gp_have_cppcheck" = xyes])
    GP_STATUS_BUILD_FEATURE_ADD([Static code checking],
                                [$gp_have_cppcheck])
])
