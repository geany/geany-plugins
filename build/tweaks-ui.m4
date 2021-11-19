AC_DEFUN([GP_CHECK_TWEAKS_UI],
[
    GP_ARG_DISABLE([Tweaks-UI], [auto])
    GP_COMMIT_PLUGIN_STATUS([Tweaks-UI])
    AC_CONFIG_FILES([
        tweaks-ui/Makefile
    ])
])
