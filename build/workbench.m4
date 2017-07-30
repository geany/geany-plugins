AC_DEFUN([GP_CHECK_WORKBENCH],
[
    GP_ARG_DISABLE([Workbench], [auto])
    GP_COMMIT_PLUGIN_STATUS([Workbench])
    AC_CONFIG_FILES([
        workbench/Makefile
        workbench/src/Makefile
        workbench/icons/Makefile
    ])
])
