AC_DEFUN([GP_CHECK_TREEBROWSER],
[
    GP_ARG_DISABLE([Treebrowser], [auto])
    if [[ "$enable_treebrowser" != no ]]; then
        AC_CHECK_FUNC([creat],,
            [
                if [[ "$enable_treebrowser" = auto ]]; then
                    enable_treebrowser=no
                else
                    AC_MSG_ERROR([Treebrowser cannot be enabled because creat() is missing.
                                  Please disable it (--disable-treebrowser) or make sure creat()
                                  works on your system.])
                fi
            ])

    PKG_CHECK_MODULES([GIO], [gio-2.0],
        [AC_DEFINE([HAVE_GIO], 1, [Whether we have GIO])],
        [AC_MSG_NOTICE([Treebrowser GIO support is disabled because of the following problem: $GIO1_PKG_ERRORS])])

    if test "x$enable_treebrowser" = "xauto"; then
        enable_treebrowser="$treebrowser_have_creat"
    elif test "x$enable_treebrowser" = "xyes"; then
        if ! test "x$treebrowser_have_creat" = "xyes"; then
            AC_MSG_ERROR([missing function creat()])
        fi
    fi

    AM_CONDITIONAL(ENABLE_TREEBROWSER, test "x$enable_treebrowser" = "xyes")
    GP_STATUS_PLUGIN_ADD([TreeBrowser], [$enable_treebrowser])

    AC_CONFIG_FILES([
        treebrowser/Makefile
        treebrowser/src/Makefile
    ])
])
