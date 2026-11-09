AC_DEFUN([GP_CHECK_JACOCOCOVERAGE],
[
    GP_ARG_DISABLE([Jacococoverage], [auto])
    GP_COMMIT_PLUGIN_STATUS([Jacococoverage])
    AC_CONFIG_FILES([
        jacococoverage/Makefile
        jacococoverage/src/Makefile
    ])
])
