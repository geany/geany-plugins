AC_DEFUN([GP_CHECK_SCOPE],
[
    GP_ARG_DISABLE([Scope], [auto])
    GP_CHECK_PLUGIN_GTK3_ONLY([Scope])

    case "$host_os" in
        cygwin* | mingw* | win32*)
            PTY_LIBS=""
            ;;

        *)
            GP_CHECK_PLUGIN_DEPS([scope], [VTE],
                                 [vte-2.91])
            AC_CHECK_HEADERS([util.h pty.h libutil.h])
            PTY_LIBS="-lutil"
            ;;
    esac

    AC_SUBST(PTY_LIBS)

    GP_COMMIT_PLUGIN_STATUS([Scope])

    AC_CONFIG_FILES([
        scope/Makefile
        scope/data/Makefile
        scope/docs/Makefile
        scope/src/Makefile
    ])
])
