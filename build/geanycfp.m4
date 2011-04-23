AC_DEFUN([GP_CHECK_GEANYCFP],
[
    GP_ARG_DISABLE([geanycfp], [no])
    GP_STATUS_PLUGIN_ADD([GeanyCFP], [$enable_geanycfp])
    AC_CONFIG_FILES([
        geanycfp/Makefile
        geanycfp/src/Makefile
    ])
])
