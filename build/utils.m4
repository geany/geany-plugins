AC_DEFUN([GP_CHECK_UTILSLIB],
[
dnl
dnl TODO: only build the library if any plugins using it are enabled
dnl
    AC_ARG_ENABLE([utilslib],
              [AS_HELP_STRING([--enable-utilslib],
                              [Whether to use the utilities library [[default=auto]]])],
              [enable_utilslib=$enableval],
              [enable_utilslib=auto])
    AS_IF([test "x$enable_utilslib" != "xno"], [
        enable_utilslib=yes
        GP_STATUS_FEATURE_ADD([Utility library], [$enable_utilslib])
        AC_CONFIG_FILES([
            utils/Makefile
            utils/src/Makefile
        ])
    ])
])
