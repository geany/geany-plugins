AC_DEFUN([GP_CHECK_SPELLCHECK],
[
    GP_ARG_DISABLE([spellcheck], [auto])

    ENCHANT_VERSION=1.3
    ENCHANT_VERSION_1_5=1.5
    ENCHANT_VERSION_2_0=2.0
    PKG_CHECK_MODULES([ENCHANT_1_5], [enchant >= ${ENCHANT_VERSION_1_5}],
                      have_enchant_1_5=yes,
                      have_enchant_1_5=no)
    PKG_CHECK_MODULES([ENCHANT_2_0], [enchant >= ${ENCHANT_VERSION_2_0}],
                      have_enchant_2_0=yes,
                      have_enchant_2_0=no)
    GP_CHECK_PLUGIN_DEPS([spellcheck], [ENCHANT],
                         [enchant >= ${ENCHANT_VERSION}])

    AM_CONDITIONAL([HAVE_ENCHANT_1_5], [test "$have_enchant_1_5" = yes])
    AM_CONDITIONAL([HAVE_ENCHANT_2_0], [test "$have_enchant_2_0" = yes])
    GP_COMMIT_PLUGIN_STATUS([Spellcheck])

    AC_CONFIG_FILES([
        spellcheck/Makefile
        spellcheck/src/Makefile
    ])
])
