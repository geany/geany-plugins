AC_DEFUN([GP_CHECK_DEBUGGER],
[
    GP_ARG_DISABLE([Debugger], [auto])
    GP_CHECK_PLUGIN_GTK3_ONLY([Debugger])
    GP_CHECK_PLUGIN_DEPS([debugger], [VTE],
                         [vte-2.91 >= 0.50])
    AC_CHECK_HEADERS([util.h pty.h libutil.h])
    GP_COMMIT_PLUGIN_STATUS([Debugger])
    AC_CONFIG_FILES([
        debugger/Makefile
        debugger/src/Makefile
        debugger/img/Makefile
    ])
])
