AC_DEFUN([GP_CHECK_PAIRTAGHIGHLIGHTER],
[
    GP_ARG_DISABLE([PairTagHighlighter], [auto])
    GP_COMMIT_PLUGIN_STATUS([PairTagHighlighter])
    AC_CONFIG_FILES([
        pairtaghighlighter/Makefile
        pairtaghighlighter/src/Makefile
    ])
])
