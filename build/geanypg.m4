AC_DEFUN([GP_CHECK_GEANYPG],
[
    GP_ARG_DISABLE([geanypg], [auto])
    if test "$enable_geanypg" = "auto"; then
        enable_geanypg=no
        m4_ifdef([AM_PATH_GPGME], [AM_PATH_GPGME(, enable_geanypg=auto)])
    elif test "$enable_geanypg" = "yes"; then
        m4_ifdef([AM_PATH_GPGME],
                [AM_PATH_GPGME(,, [AC_MSG_ERROR([Could not find GPGME. Please define GPGME_CFLAGS and GPGME_LIBS if it is installed.])])],
                [AC_MSG_ERROR([Could not find GPGME. Please install it])])

    fi

    AS_IF([test "$enable_geanypg" != "no"], [
        AC_CHECK_FUNCS([fdopen],,[
            AS_IF([test "$enable_geanypg" = "yes"],
                  [AC_MSG_ERROR([Could not find fdopen])],
                  [enable_geanypg=no])
        ])
    ])

    # necessary for gpgme
    AC_SYS_LARGEFILE

    GP_COMMIT_PLUGIN_STATUS([GeanyPG])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
