AC_DEFUN([GP_CHECK_SCOPE],
[
    GP_ARG_DISABLE([Scope], [auto])

    AS_CASE([$host_os],
            [cygwin* | mingw* | win32*],
            [PTY_LIBS=""],

            [GP_CHECK_GTK3([vte_package=vte-2.91], [vte_package="vte >= 0.17"])
             GP_CHECK_PLUGIN_DEPS([scope], [VTE], [$vte_package])
             GP_CHECK_UTILSLIB_VTECOMPAT([Scope])

             AC_CHECK_HEADERS([util.h pty.h libutil.h])
             PTY_LIBS="-lutil"])

    AC_SUBST(PTY_LIBS)

    GP_COMMIT_PLUGIN_STATUS([Scope])

    AC_CONFIG_FILES([
        scope/Makefile
        scope/data/Makefile
        scope/docs/Makefile
        scope/src/Makefile
    ])
])
