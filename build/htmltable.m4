AC_DEFUN([GP_CHECK_HTMLTABLE],
[
    GP_ARG_DISABLE([HTMLTable], [yes])
    GP_STATUS_PLUGIN_ADD([HTMLTable], [$enable_htmltable])
    AC_CONFIG_FILES([
        htmltable/Makefile
        htmltable/src/Makefile
    ])
])
