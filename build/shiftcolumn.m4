AC_DEFUN([GP_CHECK_SHIFTCOLUMN],
[
    GP_ARG_DISABLE([ShiftColumn], [yes])
    GP_COMMIT_PLUGIN_STATUS([ShiftColumn])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
