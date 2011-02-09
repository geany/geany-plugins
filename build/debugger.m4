AC_DEFUN([GP_CHECK_DEBUGGER],
[
    GP_ARG_DISABLE([Debugger], [auto])
    GP_CHECK_PLUGIN_DEPS([debugger], [VTE],
                         [vte >= 0.24])
    GP_STATUS_PLUGIN_ADD([Debugger], [$enable_debugger])
    AC_CONFIG_FILES([
        debugger/Makefile
        debugger/src/Makefile
    ])
])
