AC_DEFUN([GP_CHECK_LATEX],
[
    GP_ARG_DISABLE([LaTeX], [auto])
    GP_COMMIT_PLUGIN_STATUS([LaTeX])
    AC_CONFIG_FILES([
        latex/Makefile
        latex/doc/Makefile
        latex/src/Makefile
    ])
])
