AC_DEFUN([GP_CHECK_WORKBENCH],
[
    GP_ARG_DISABLE([Workbench], [auto])
    GP_CHECK_UTILSLIB([Workbench])

    PKG_CHECK_MODULES([GIO], [gio-2.0],
        [AC_DEFINE([HAVE_GIO], 1, [Whether we have GIO])
         have_gio=yes],
        [have_gio=no])

    GP_COMMIT_PLUGIN_STATUS([Workbench])
    AC_CONFIG_FILES([
        workbench/Makefile
        workbench/src/Makefile
    ])
])
