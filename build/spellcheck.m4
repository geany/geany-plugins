AC_DEFUN([GP_CHECK_SPELLCHECK],
[
    GP_ARG_DISABLE([spellcheck], [auto])

    ENCHANT_VERSION=1.3
    OPT_ENCHANT_VERSION=1.5
    GP_CHECK_PLUGIN_DEPS([spellcheck], [ENCHANT],
                         [enchant >= ${ENCHANT_VERSION}])
    PKG_CHECK_MODULES([ENCHANT], [enchant >= ${OPT_ENCHANT_VERSION}],
                      have_enchant_1_5=yes,
                      have_enchant_1_5=no)

    AM_CONDITIONAL([HAVE_ENCHANT_1_5], [test "$have_enchant_1_5" = yes])
    GP_STATUS_PLUGIN_ADD([Spellcheck], [$enable_spellcheck])

    AC_CONFIG_FILES([
        spellcheck/Makefile
        spellcheck/src/Makefile
    ])
])
