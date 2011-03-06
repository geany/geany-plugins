AC_DEFUN([GP_CHECK_GEANYPG],
[
    GP_ARG_DISABLE([geanypg], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyPG], [$enable_geanypg])
    AM_PATH_GPGME
    AC_CONFIG_FILES([
        geanypg/Makefile
        geanypg/src/Makefile
    ])
])
