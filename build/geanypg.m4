AC_DEFUN([GP_CHECK_GEANYPG],
[
    GP_ARG_DISABLE([GeanyPG], [auto])
    GP_CHECK_UTILSLIB([GeanyPG])

    GP_CHECK_PLUGIN_DEPS([GeanyPG], [GEANYPG],
                         [gpgme >= 1.23])
    # necessary for gpgme
    AC_SYS_LARGEFILE
    GP_COMMIT_PLUGIN_STATUS([GeanyPG])
    AC_CONFIG_FILES([
        geanypg/Makefile
        geanypg/src/Makefile
    ])
])
