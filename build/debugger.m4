AC_DEFUN([GP_CHECK_DEBUGGER],
[
    GP_ARG_DISABLE([Debugger], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([Debugger])
    GP_CHECK_PLUGIN_DEPS([debugger], [VTE],
                         [vte >= 0.24])
    GP_COMMIT_PLUGIN_STATUS([Debugger])
    AC_CONFIG_FILES([
        debugger/Makefile
        debugger/src/Makefile
        debugger/img/Makefile
    ])
])
