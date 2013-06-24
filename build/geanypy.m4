AC_DEFUN([GP_CHECK_GEANYPY],
[
    GP_ARG_DISABLE([Geanypy], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([Geanypy])
    GP_CHECK_PLUGIN_DEPS([Geanypy], [PYGTK], [pygtk-2.0])
    dnl FIXME: Checks for Python below should gracefully disable the plugin
    dnl        if they don't succeed and enable_geanypy is set to `auto`.
    dnl        However, since these macros don't seem to gracefully handle
    dnl        failure, and since if PyGTK is found they are likely to succeed
    dnl        anyway, we assume that if the plugin is not disabled at this
    dnl        point it is OK for these checks to be fatal.  The user can pass
    dnl        always pass --disable-geanypy anyway.
    AS_IF([! test x$enable_geanypy = xno], [
        AX_PYTHON_DEVEL([>= '2.6'])
        AX_PYTHON_LIBRARY(,[AC_MSG_ERROR([Cannot find Python library])])
        AC_SUBST([PYTHON])
        AC_DEFINE_UNQUOTED([GEANYPY_PYTHON_LIBRARY],
                           ["$PYTHON_LIBRARY"],
                           [Location of Python library to dlopen()])
    ])
    GP_COMMIT_PLUGIN_STATUS([Geanypy])
    AC_CONFIG_FILES([
        geanypy/Makefile
        geanypy/src/Makefile
        geanypy/geany/Makefile
        geanypy/plugins/Makefile
    ])
])
