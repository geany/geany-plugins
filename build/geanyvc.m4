AC_DEFUN([GP_CHECK_GEANYVC],
[
    GP_STATUS_PLUGIN_ADD([GeanyVC], [yes])
    AC_ARG_ENABLE(gtkspell,
        AC_HELP_STRING([--enable-gtkspell=ARG],
            [Enable GtkSpell support in GeanyVC. [[default=auto]]]),,
        enable_gtkspell=auto)

    if [[ x"$enable_gtkspell" = "xauto" ]]; then
        PKG_CHECK_MODULES(GTKSPELL, gtkspell-2.0,
            enable_gtkspell=yes, enable_gtkspell=no)
    elif [[ x"$enable_gtkspell" = "xyes" ]]; then
        PKG_CHECK_MODULES(GTKSPELL, [gtkspell-2.0])
    fi

    AM_CONDITIONAL(USE_GTKSPELL, test $enable_gtkspell = yes)
    GP_STATUS_FEATURE_ADD([GeanyVC GtkSpell support], [$enable_gtkspell])

    AC_CONFIG_FILES([
        geanyvc/Makefile
        geanyvc/src/Makefile
    ])
])
