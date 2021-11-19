AC_DEFUN([_GP_UTILSLIB_ARG],
[
    AC_ARG_ENABLE([utilslib],
              [AS_HELP_STRING([--enable-utilslib],
                              [Whether to use the utilities library [[default=auto]]])],
              [enable_utilslib=$enableval],
              [enable_utilslib=auto])
])

dnl GP_CHECK_UTILSLIB_VTECOMPAT(PluginName)
AC_DEFUN([GP_CHECK_UTILSLIB_VTECOMPAT],
[
    AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" != no],
          [GP_CHECK_GTK3([utilslib_vte_package=vte-2.91],
                         [utilslib_vte_package=vte])
           PKG_CHECK_MODULES([UTILSLIB], [$utilslib_vte_package],
                             [utilslib_have_vte=yes],
                             [utilslib_have_vte=no])
           AS_IF([test "$utilslib_have_vte" != yes || test "$enable_utilslib" = no],
                 [AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = yes],
                        [AC_MSG_ERROR([Plugin $1 depends on utilslib VTE support which is not available])],
                        [m4_tolower(AS_TR_SH(enable_$1))=no])],
                 [enable_utilslib=yes])])
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
    AM_CONDITIONAL([ENABLE_UTILSLIB_VTECOMPAT], [test "$enable_utilslib" = yes && test "$utilslib_have_vte" = yes])
    GP_STATUS_FEATURE_ADD([Utility library], [$enable_utilslib])
    GP_STATUS_FEATURE_ADD([Utility library VTE support], [${utilslib_have_vte-no}])
])
