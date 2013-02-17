AC_DEFUN([GP_CHECK_GEANYPG],
[
    GP_ARG_DISABLE([geanypg], [auto])
    if test "$enable_geanypg" = "auto"; then
        enable_geanypg=no
        m4_ifdef([AM_PATH_GPGME], [AM_PATH_GPGME(, enable_geanypg=yes)])
    elif test "$enable_geanypg" = "yes"; then
        m4_ifdef([AM_PATH_GPGME],
                [AM_PATH_GPGME(,, [AC_MSG_ERROR([Could not find GPGME. Please define GPGME_CFLAGS and GPGME_LIBS if it is installed.])])],
                [AC_MSG_ERROR([Could not find GPGME. Please install it])])

    fi

    AM_CONDITIONAL(ENABLE_GEANYPG, test "$enable_geanypg" = "yes")
    # necessary for gpgme
    AC_SYS_LARGEFILE

    GP_COMMIT_PLUGIN_STATUS([GeanyPG])
    AC_CONFIG_FILES([
        geanypg/Makefile
        geanypg/src/Makefile
    ])
])
