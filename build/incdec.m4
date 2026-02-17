AC_DEFUN([GP_CHECK_INCDEC],
[
    GP_ARG_DISABLE([IncDec], [auto])

    GP_COMMIT_PLUGIN_STATUS([IncDec])

    AC_CONFIG_FILES([
        incdec/Makefile
        incdec/src/Makefile
    ])
])
