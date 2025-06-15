AC_DEFUN([GP_CHECK_SPELLCHECK],
[
    GP_ARG_DISABLE([spellcheck], [auto])

    ENCHANT_PACKAGE_NAME=enchant-2
    ENCHANT_VERSION=1.3
    ENCHANT_VERSION_1_5=1.5
    ENCHANT_VERSION_2_0=2.0
    ENCHANT_VERSION_2_2=2.2

    # check for enchant package
    PKG_CHECK_MODULES([ENCHANT_2_2], [${ENCHANT_PACKAGE_NAME} >= ${ENCHANT_VERSION_2_2}],
                      have_enchant_2_2=yes,
                      have_enchant_2_2=no)
    if [[ x"$have_enchant_2_2" = "xyes" ]]; then
        # we have got the new enchant-2 package
        have_enchant_1_5=yes
        have_enchant_2_0=yes
    else
        # check for old enchant package
        PKG_CHECK_MODULES([ENCHANT_1_5], [enchant >= ${ENCHANT_VERSION_1_5}],
                          have_enchant_1_5=yes,
                          have_enchant_1_5=no)
        PKG_CHECK_MODULES([ENCHANT_2_0], [enchant >= ${ENCHANT_VERSION_2_0}],
                          have_enchant_2_0=yes,
                          have_enchant_2_0=no)

        ENCHANT_PACKAGE_NAME=enchant
    fi

    GP_CHECK_PLUGIN_DEPS([spellcheck], [ENCHANT],
                         [${ENCHANT_PACKAGE_NAME} >= ${ENCHANT_VERSION}])

    AM_CONDITIONAL([HAVE_ENCHANT_1_5], [test "$have_enchant_1_5" = yes])
    AM_CONDITIONAL([HAVE_ENCHANT_2_0], [test "$have_enchant_2_0" = yes])
    GP_COMMIT_PLUGIN_STATUS([Spellcheck])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
