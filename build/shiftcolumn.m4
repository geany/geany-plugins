AC_DEFUN([GP_CHECK_SHIFTCOLUMN],
[
    GP_ARG_DISABLE([ShiftColumn], [yes])
    GP_STATUS_PLUGIN_ADD([ShiftColumn], [$enable_shiftcolumn])
    AC_CONFIG_FILES([
        shiftcolumn/Makefile
        shiftcolumn/src/Makefile
    ])
])
