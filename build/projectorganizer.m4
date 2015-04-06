AC_DEFUN([GP_CHECK_PROJECTORGANIZER],
[
    GP_ARG_DISABLE([ProjectOrganizer], [auto])
    GP_CHECK_PLUGIN_GEANY_VERSION([ProjectOrganizer], [1.25])
    GP_COMMIT_PLUGIN_STATUS([ProjectOrganizer])
    AC_CONFIG_FILES([
        projectorganizer/Makefile
        projectorganizer/src/Makefile
        projectorganizer/icons/Makefile
    ])
])
