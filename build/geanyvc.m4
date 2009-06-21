AC_DEFUN([GP_CHECK_GEANYVC],
[
    AC_ARG_ENABLE([gtkspell],
        AC_HELP_STRING([--disable-gtkspell],
            [Disables gtkspell support in GeanyVC]),
        [enable_gtkspell=$enableval],
        [enable_gtkspell=yes])

    if [[ x"$enable_gtkspell" = "xyes" ]]; then
        PKG_CHECK_MODULES(GTKSPELL, [gtkspell-2.0],
            [AM_CONDITIONAL([USE_GTKSPELL], true)],
            [AM_CONDITIONAL([USE_GTKSPELL], false)])
    else
        AM_CONDITIONAL([USE_GTKSPELL], false)
    fi

    AC_CONFIG_FILES([
        geanyvc/Makefile
        geanyvc/src/Makefile
    ])
])
