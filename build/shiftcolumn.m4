AC_DEFUN([GP_CHECK_SHIFTCOLUMN],
[
    GP_STATUS_PLUGIN_ADD([ShiftColumn], [yes])
    AC_CONFIG_FILES([
        shiftcolumn/Makefile
        shiftcolumn/src/Makefile
    ])
])
