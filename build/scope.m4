AC_DEFUN([GP_CHECK_SCOPE],
[
    GP_ARG_DISABLE([Scope], [yes])

    GP_CHECK_PLUGIN_DEPS([scope], [VTE],
                         [vte >= 0.17])

    GP_COMMIT_PLUGIN_STATUS([Scope])

    case "$host_os" in
        cygwin* | mingw* | win32*) PTY_LIBS="" ;;
        *) PTY_LIBS="-lutil" ;;
    esac

    AC_SUBST(PTY_LIBS)

    AC_CONFIG_FILES([
        scope/Makefile
        scope/data/Makefile
        scope/docs/Makefile
        scope/src/Makefile
    ])
])
