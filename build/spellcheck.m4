AC_DEFUN([GP_CHECK_SPELLCHECK],
[
    AC_ARG_ENABLE([spellcheck],
        AC_HELP_STRING([--enable-spellcheck=ARG],
            [Enable the spellcheck plugin [[default=auto]]]),,
        enable_spellcheck=auto)

    ENCHANT_VERSION=1.3

    if [[ x"$enable_spellcheck" = "xauto" ]]; then
        PKG_CHECK_MODULES(ENCHANT, [enchant >= $ENCHANT_VERSION],
            [enable_spellcheck=yes],
            [enable_spellcheck=no])
    elif [[ x"$enable_spellcheck" = "xyes" ]]; then
        PKG_CHECK_MODULES(ENCHANT, [enchant >= $ENCHANT_VERSION])
    fi

    AM_CONDITIONAL(ENABLE_SPELLCHECK, test $enable_spellcheck = yes)
    GP_STATUS_PLUGIN_ADD([Spellcheck], [$enable_spellcheck])

    AC_CONFIG_FILES([
        spellcheck/Makefile
        spellcheck/src/Makefile
    ])
])
