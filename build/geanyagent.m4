AC_DEFUN([GP_CHECK_GEANYAGENT],
[
    GP_ARG_DISABLE([Geanyagent], [auto])

    AS_CASE([$host_os],
            [cygwin* | mingw* | win32*],
            [],

            [GP_CHECK_GTK3([vte_package=vte-2.91 vte_version=0.48],
                           [vte_package=vte vte_version=0.38])
             GP_CHECK_PLUGIN_DEPS([geanyagent], [VTE],
                                  [${vte_package} >= ${vte_version}])])

    GP_COMMIT_PLUGIN_STATUS([Geanyagent])

    AC_CONFIG_FILES([
        geanyagent/Makefile
        geanyagent/src/Makefile
    ])
])
