AC_DEFUN([GP_CHECK_XMLSNIPPETS],
[
    GP_ARG_DISABLE(XMLSnippets, yes)
    GP_COMMIT_PLUGIN_STATUS([XMLSnippets])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
