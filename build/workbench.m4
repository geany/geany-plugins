AC_DEFUN([GP_CHECK_WORKBENCH],
[
    GP_ARG_DISABLE([Workbench], [auto])
    GP_CHECK_UTILSLIB([Workbench])

    GP_CHECK_PLUGIN_DEPS([Workbench], [WORKBENCH],
                         [libgit2 >= 0.21])

    GP_COMMIT_PLUGIN_STATUS([Workbench])
    AC_CONFIG_FILES([
        workbench/Makefile
        workbench/src/Makefile
    ])
])
