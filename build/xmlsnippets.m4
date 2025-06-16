AC_DEFUN([GP_CHECK_XMLSNIPPETS],
[
    GP_ARG_DISABLE(XMLSnippets, yes)
    GP_COMMIT_PLUGIN_STATUS([XMLSnippets])
    AC_CONFIG_FILES([
        xmlsnippets/Makefile
        xmlsnippets/src/Makefile
    ])
])
