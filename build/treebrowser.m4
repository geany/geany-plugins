AC_DEFUN([GP_CHECK_TREEBROWSER],
[
    GP_ARG_DISABLE([Treebrowser], [auto])
    GP_CHECK_UTILSLIB([Treebrowser])

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

    AM_CONDITIONAL(ENABLE_TREEBROWSER, test "x$enable_treebrowser" = "xyes")
    GP_COMMIT_PLUGIN_STATUS([TreeBrowser])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
