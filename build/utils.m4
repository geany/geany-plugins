AC_DEFUN([_GP_UTILSLIB_ARG],
[
    AC_ARG_ENABLE([utilslib],
              [AS_HELP_STRING([--enable-utilslib],
                              [Whether to use the utilities library [[default=auto]]])],
              [enable_utilslib=$enableval],
              [enable_utilslib=auto])
])

dnl GP_CHECK_UTILSLIB(PluginName)
dnl Check for utils library
AC_DEFUN([GP_CHECK_UTILSLIB],
[
    AC_REQUIRE([_GP_UTILSLIB_ARG])
    AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" != no &&
           test "$enable_utilslib" != yes],
          [AS_IF([test "$enable_utilslib" = "no"],
                 [AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = yes],
                        [AC_MSG_ERROR([Plugin $1 depends on utilslib which is disabled])],
                        [m4_tolower(AS_TR_SH(enable_$1))=no])],
                 [enable_utilslib=yes])])
])

AC_DEFUN([GP_COMMIT_UTILSLIB_STATUS],
[
    AS_IF([test "$enable_utilslib" = "yes"],
          [AC_CONFIG_FILES([
               utils/Makefile
               utils/src/Makefile
           ])],
          [enable_utilslib=no])
    AM_CONDITIONAL([ENABLE_UTILSLIB], [test "$enable_utilslib" = "yes"])
    GP_STATUS_FEATURE_ADD([Utility library], [$enable_utilslib])
])
