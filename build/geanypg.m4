AC_DEFUN([GP_CHECK_GEANYPG],
[
    GP_ARG_DISABLE([geanypg], [auto])
    if test "$enable_geanypg" = "auto"; then
        AM_PATH_GPGME(, enable_geanypg=yes, enable_geanypg=no)
    else
        AM_PATH_GPGME
    fi

    AM_CONDITIONAL(ENABLE_GEANYPG, test "$enable_geanypg" = "yes")

    GP_STATUS_PLUGIN_ADD([GeanyPG], [$enable_geanypg])
    AC_CONFIG_FILES([
        geanypg/Makefile
        geanypg/src/Makefile
    ])
])
