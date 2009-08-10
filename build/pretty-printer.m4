AC_DEFUN([GP_CHECK_PRETTYPRINTER],
[
    AC_ARG_ENABLE([prettyprinter],
        AC_HELP_STRING([--enable-prettyprinter=ARG],
            [Enable the pretty-printer plugin [[default=auto]]]),,
        enable_prettyprinter=auto)

    LIBXML_VERSION=2.6.27
    if [[ x"$enable_prettyprinter" = "xauto" ]]; then
       PKG_CHECK_MODULES(LIBXML, [libxml-2.0 >= $LIBXML_VERSION],
           [enable_prettyprinter=yes],
           [enable_prettyprinter=no])
    elif [[ x"$enable_prettyprinter" = "xyes" ]]; then
       PKG_CHECK_MODULES(LIBXML, [libxml-2.0 >= $LIBXML_VERSION])
    fi

    AM_CONDITIONAL(ENABLE_PRETTYPRINTER, test $enable_prettyprinter = yes)

    GP_STATUS_PLUGIN_ADD([Pretty Printer], [$enable_prettyprinter])

    AC_CONFIG_FILES([
        pretty-printer/Makefile
        pretty-printer/src/Makefile
    ])
])
