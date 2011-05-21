AC_DEFUN([GP_CHECK_XMLSNIPPETS],
[
    GP_ARG_DISABLE(XMLSnippets, yes)
    GP_STATUS_PLUGIN_ADD([XMLSnippets], [$enable_xmlsnippets])
    AC_CONFIG_FILES([
        xmlsnippets/Makefile
        xmlsnippets/src/Makefile
    ])
])
