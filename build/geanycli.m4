AC_DEFUN([GP_CHECK_GEANYCLI],
[
    GP_ARG_DISABLE([Geanycli], [auto])

    AS_CASE([$host_os],
            [cygwin* | mingw* | win32*],
            [],

            [GP_CHECK_GTK3([vte_package=vte-2.91 vte_version=0.48],
                           [vte_package=vte vte_version=0.38])
             GP_CHECK_PLUGIN_DEPS([geanycli], [VTE],
                                  [${vte_package} >= ${vte_version}])])

    GP_COMMIT_PLUGIN_STATUS([Geanycli])

    AC_CONFIG_FILES([
        geanycli/Makefile
        geanycli/src/Makefile
    ])
])
