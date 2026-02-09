AC_DEFUN([GP_CHECK_EXTERNALTOOLS],
[
    GP_ARG_DISABLE([ExternalTools], [auto])
    GP_COMMIT_PLUGIN_STATUS([ExternalTools])
    AC_CONFIG_FILES([
        externaltools/Makefile
        externaltools/src/Makefile
    ])
])
