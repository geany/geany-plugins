AC_DEFUN([GP_CHECK_LSP],
[
    GP_ARG_DISABLE([LSP], [auto])
    GP_COMMIT_PLUGIN_STATUS([LSP])
    AC_CONFIG_FILES([
        lsp/Makefile
        lsp/deps/Makefile
        lsp/src/Makefile
        lsp/data/Makefile
    ])
])
