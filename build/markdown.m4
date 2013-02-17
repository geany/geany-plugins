AC_DEFUN([GP_CHECK_MARKDOWN],
[
    GP_ARG_DISABLE([markdown], [auto])

    GTK_VERSION=2.16
    WEBKIT_VERSION=1.1.13

    GP_CHECK_PLUGIN_DEPS([markdown], [MARKDOWN],
                         [gtk+-2.0 >= ${GTK_VERSION}
                          webkit-1.0 >= ${WEBKIT_VERSION}
                          gthread-2.0])

    GP_COMMIT_PLUGIN_STATUS([Markdown])

    AC_CONFIG_FILES([
        markdown/Makefile
        markdown/discount/Makefile
        markdown/src/Makefile
        markdown/docs/Makefile
    ])
])
