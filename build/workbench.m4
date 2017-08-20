AC_DEFUN([GP_CHECK_WORKBENCH],
[
    GP_ARG_DISABLE([Workbench], [auto])
    GP_COMMIT_PLUGIN_STATUS([Workbench])
    AC_CONFIG_FILES([
        workbench/Makefile
        workbench/src/Makefile
        workbench/icons/Makefile
        workbench/icons/16x16/Makefile
        workbench/icons/24x24/Makefile
        workbench/icons/32x32/Makefile
        workbench/icons/48x48/Makefile
        workbench/icons/scalable/Makefile
    ])
])
