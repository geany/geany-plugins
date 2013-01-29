AC_DEFUN([GP_CHECK_SHIFTCOLUMN],
[
    GP_ARG_DISABLE([ShiftColumn], [yes])
    GP_COMMIT_PLUGIN_STATUS([ShiftColumn])
    AC_CONFIG_FILES([
        shiftcolumn/Makefile
        shiftcolumn/src/Makefile
    ])
])
