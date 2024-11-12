AC_DEFUN([GP_CHECK_BRACKETCOLORS],
[
    GP_ARG_DISABLE([Bracketcolors], [auto])
    GP_CHECK_PLUGIN_GTK3_ONLY([Bracketcolors])
    GP_CHECK_PLUGIN_DEPS([Bracketcolors], [BRACKETCOLORS], [geany >= 1.38])

    dnl Check for c++17. We really only need c++11 but scintilla is
    dnl compiled with c++17 so just use that

    AS_IF(
        [test x$enable_bracketcolors = xauto], [
            AX_CXX_COMPILE_STDCXX_17(, optional)
            AS_IF(
                [test ${HAVE_CXX17} = 1], [
                    enable_bracketcolors=yes
                ],
                [enable_bracketcolors=no]
            )
        ],
        [test x$enable_bracketcolors = xyes], [
            AX_CXX_COMPILE_STDCXX_17(, mandatory)
        ]
    )

    GP_COMMIT_PLUGIN_STATUS([Bracketcolors])
    AC_CONFIG_FILES([
        bracketcolors/Makefile
        bracketcolors/src/Makefile
    ])
])

