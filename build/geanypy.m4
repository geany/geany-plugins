AC_DEFUN([GP_CHECK_GEANYPY],
[
    GP_ARG_DISABLE([Geanypy], [auto])
    GP_CHECK_PLUGIN_GTK2_ONLY([Geanypy])
    GP_COMMIT_PLUGIN_STATUS([Geanypy])
    PKG_CHECK_MODULES([PYGTK], [pygtk-2.0])
    AX_PYTHON_DEVEL([>= '2.6'])
    AX_PYTHON_LIBRARY(,[AC_MSG_ERROR([Cannot find Python library])])
    AC_SUBST([PYTHON])
	AC_DEFINE_UNQUOTED(
	    [GEANYPY_PYTHON_LIBRARY],
	    ["$PYTHON_LIBRARY"],
	    [Location of Python library to dlopen()])
    AC_CONFIG_FILES([
        geanypy/Makefile
        geanypy/src/Makefile
        geanypy/geany/Makefile
        geanypy/plugins/Makefile
    ])
])
