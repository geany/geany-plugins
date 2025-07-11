AC_DEFUN([GP_CHECK_GEANYVC],
[
    GP_ARG_DISABLE([GeanyVC], [auto])
    GP_COMMIT_PLUGIN_STATUS([GeanyVC])
    AC_ARG_ENABLE(gtkspell,
        AS_HELP_STRING([--enable-gtkspell=ARG],
            [Enable GtkSpell support in GeanyVC. [[default=auto]]]),,
        enable_gtkspell=auto)
    GP_CHECK_GTK3([gtkspell_package=gtkspell3-3.0],
                  [gtkspell_package=gtkspell-2.0])
    if [[ x"$enable_gtkspell" = "xauto" ]]; then
        PKG_CHECK_MODULES(GTKSPELL, $gtkspell_package,
            enable_gtkspell=yes, enable_gtkspell=no)
    elif [[ x"$enable_gtkspell" = "xyes" ]]; then
        PKG_CHECK_MODULES(GTKSPELL, [$gtkspell_package])
    fi
    if [[ x"$enable_gtkspell" = "xyes" ]]; then
        AC_DEFINE(USE_GTKSPELL, 1, [GtkSpell support])
    fi

    if [[ "$enable_gtkspell" = yes -a "$enable_geanyvc" = no ]]; then
       AC_MSG_WARN([GtkSpell support for GeanyVC enabled, but GeanyVC itself not enabled.])
    fi

    GP_STATUS_FEATURE_ADD([GeanyVC GtkSpell support], [$enable_gtkspell])

    AC_CONFIG_FILES([
        geanyvc/Makefile
        geanyvc/src/Makefile
    ])
])
