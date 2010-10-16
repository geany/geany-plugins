AC_DEFUN([GP_CHECK_TREEBROWSER],
[
    PKG_CHECK_MODULES([GIO], [gio-2.0],
                      [AC_DEFINE([HAVE_GIO], 1, [Whether we have GIO])], [])
    GP_STATUS_PLUGIN_ADD([Treebrowser], [yes])
    AC_CONFIG_FILES([
        treebrowser/Makefile
        treebrowser/src/Makefile
    ])
])
