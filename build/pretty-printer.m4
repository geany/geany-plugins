AC_DEFUN([GP_CHECK_PRETTYPRINTER],
[
    LIBXML_VERSION=2.6.27

    GP_ARG_DISABLE([pretty-printer], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([pretty-printer])
    GP_CHECK_PLUGIN_DEPS([pretty-printer], [LIBXML],
                         [libxml-2.0 >= ${LIBXML_VERSION}])
    GP_COMMIT_PLUGIN_STATUS([Pretty Printer])

    AC_CONFIG_FILES([
        pretty-printer/Makefile
        pretty-printer/src/Makefile
    ])
])
