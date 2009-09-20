AC_DEFUN([GP_CHECK_SPELLCHECK],
[
    AC_ARG_ENABLE([spellcheck],
        AC_HELP_STRING([--enable-spellcheck=ARG],
            [Enable the spellcheck plugin [[default=auto]]]),,
        enable_spellcheck=auto)

    ENCHANT_VERSION=1.3
    OPT_ENCHANT_VERSION=1.5

    if [[ x"$enable_spellcheck" = "xauto" ]]; then
        PKG_CHECK_MODULES(ENCHANT, [enchant >= $OPT_ENCHANT_VERSION],
            [have_enchant_1_5=yes; enable_spellcheck=yes],
            [have_enchant_1_5=no; enable_spellcheck=auto])
        if [[ "$enable_spellcheck" = "auto" ]]; then
            PKG_CHECK_MODULES(ENCHANT, [enchant >= $ENCHANT_VERSION],
                [enable_spellcheck=yes],
                [enable_spellcheck=no])
        fi
    elif [[ x"$enable_spellcheck" = "xyes" ]]; then
        PKG_CHECK_MODULES(ENCHANT, [enchant >= $OPT_ENCHANT_VERSION],
            [have_enchant_1_5=yes],
            [have_enchant_1_5=no;
             PKG_CHECK_MODULES(ENCHANT, [enchant >= $ENCHANT_VERSION])])
    fi

    AM_CONDITIONAL(ENABLE_SPELLCHECK, test $enable_spellcheck = yes)
    AM_CONDITIONAL(HAVE_ENCHANT_1_5, test $have_enchant_1_5 = yes)
    GP_STATUS_PLUGIN_ADD([Spellcheck], [$enable_spellcheck])

    AC_CONFIG_FILES([
        spellcheck/Makefile
        spellcheck/src/Makefile
    ])
])
