AC_DEFUN([GP_CHECK_PLAYDATE],
[
    GP_ARG_DISABLE([playdate], [auto])
    GP_COMMIT_PLUGIN_STATUS([Playdate])
    AC_CONFIG_FILES([
        playdate/Makefile
        playdate/src/Makefile
    ])
])
