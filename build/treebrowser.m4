AC_DEFUN([GP_CHECK_TREEBROWSER],
[
    GP_ARG_DISABLE([Treebrowser], [auto])
    if [[ "$enable_treebrowser" != no ]]; then
        AC_CHECK_FUNC([creat],
            [enable_treebrowser=yes],
            [
                if [[ "$enable_treebrowser" = auto ]]; then
                    enable_treebrowser=no
                else
                    AC_MSG_ERROR([Treebrowser cannot be enabled because creat() is missing.
                                  Please disable it (--disable-treebrowser) or make sure creat()
                                  works on your system.])
                fi
            ])
    fi

    PKG_CHECK_MODULES([GIO], [gio-2.0],
        [AC_DEFINE([HAVE_GIO], 1, [Whether we have GIO])
         have_gio=yes],
        [have_gio=no])

    AM_CONDITIONAL(ENABLE_TREEBROWSER, test "x$enable_treebrowser" = "xyes")
    GP_STATUS_PLUGIN_ADD([TreeBrowser], [$enable_treebrowser])
    GP_STATUS_FEATURE_ADD([TreeBrowser GIO support], [$have_gio])

    AC_CONFIG_FILES([
        treebrowser/Makefile
        treebrowser/src/Makefile
    ])
])
