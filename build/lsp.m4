AC_DEFUN([GP_CHECK_LSP],
[
    GP_ARG_DISABLE([LSP], [auto])

    JSON_GLIB_PACKAGE_NAME=json-glib-1.0
    JSON_GLIB_VERSION=1.10
    JSONRPC_GLIB_PACKAGE_NAME=jsonrpc-glib-1.0
    JSONRPC_GLIB_VERSION=3.44

    AC_ARG_ENABLE(system-jsonrpc,
        AC_HELP_STRING([--enable-system-jsonrpc],
            [Force using system json-glib and jsonrpc-glib libraries for the LSP plugin. [[default=no]]]),,
        enable_system_jsonrpc=no)

    PKG_CHECK_MODULES([SYSTEM_JSONRPC],
        [${JSON_GLIB_PACKAGE_NAME} >= ${JSON_GLIB_VERSION}
         ${JSONRPC_GLIB_PACKAGE_NAME} >= ${JSONRPC_GLIB_VERSION}],
         have_system_jsonrpc=yes
         echo "Required system versions of json-glib and jsonrpc-glib found - using them.",
         have_system_jsonrpc=no
         echo "Required system versions of json-glib and jsonrpc-glib not found - using builtin versions.")

    AS_IF([test x"$enable_system_jsonrpc" = "xyes" || test x"$have_system_jsonrpc" = "xyes"],
        [GP_CHECK_PLUGIN_DEPS([LSP], [LSP],
            [${JSON_GLIB_PACKAGE_NAME} >= ${JSON_GLIB_VERSION}
             ${JSONRPC_GLIB_PACKAGE_NAME} >= ${JSONRPC_GLIB_VERSION}])],
        [])

    AM_CONDITIONAL(ENABLE_BUILTIN_JSONRPC, [ test x"$have_system_jsonrpc" = "xno" ])

    GP_COMMIT_PLUGIN_STATUS([LSP])

    AC_CONFIG_FILES([
        lsp/Makefile
        lsp/deps/Makefile
        lsp/src/Makefile
        lsp/data/Makefile
    ])
])
