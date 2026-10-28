AC_DEFUN([GP_CHECK_GEANYCONTROL],
[
    GP_ARG_DISABLE([Geanycontrol], [auto])
    GP_COMMIT_PLUGIN_STATUS([Geanycontrol])
    AC_CONFIG_FILES([
        geanycontrol/Makefile
        geanycontrol/src/Makefile
    ])
])
