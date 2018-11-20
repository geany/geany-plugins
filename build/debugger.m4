AC_DEFUN([GP_CHECK_DEBUGGER],
[
    GP_ARG_DISABLE([Debugger], [auto])

    GP_CHECK_UTILSLIB([Debugger])
    GP_CHECK_UTILSLIB_VTECOMPAT([Debugger])
    GP_CHECK_GTK3([vte_package=vte-2.91 vte_version=0.46],
                  [vte_package=vte vte_version=0.24])
    GP_CHECK_PLUGIN_DEPS([debugger], [VTE],
                         [${vte_package} >= ${vte_version}])
    AC_CHECK_HEADERS([util.h pty.h libutil.h])

    GP_COMMIT_PLUGIN_STATUS([Debugger])
    AC_CONFIG_FILES([
        debugger/Makefile
        debugger/src/Makefile
        debugger/img/Makefile
    ])
])
