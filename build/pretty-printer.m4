AC_DEFUN([GP_CHECK_PRETTYPRINTER],
[
    LIBXML_VERSION=2.6.27

    GP_ARG_DISABLE([pretty-printer], [auto])
    GP_CHECK_PLUGIN_DEPS([pretty-printer], [LIBXML],
                         [libxml-2.0 >= ${LIBXML_VERSION}])
    GP_COMMIT_PLUGIN_STATUS([Pretty Printer])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
