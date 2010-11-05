dnl GP_ARG_DISABLE(PluginName, default)
dnl - default can either be yes(enabled) or no(disabled)
dnl Generates --enable/disable options with help strings

AC_DEFUN([GP_ARG_DISABLE],
[
    AC_ARG_ENABLE(m4_tolower(AS_TR_SH($1)),
        AS_HELP_STRING(m4_join(-,
                               --m4_if($2, no, enable, disable),
                               m4_tolower(AS_TR_SH($1))),
                       [Do not build the $1 plugin]),
        m4_tolower(AS_TR_SH(enable_$1))=$enableval,
        m4_tolower(AS_TR_SH(enable_$1))=$2)

    dnl if enableval is not auto, then we make the decision here. Otherwise this
    dnl this needs to be done in GP_CHECK_PLUGIN_DEPS
    if test "$m4_tolower(AS_TR_SH(enable_$1))" != "auto"; then
        AM_CONDITIONAL(m4_toupper(AS_TR_SH(ENABLE_$1)),
                       test "m4_tolower($AS_TR_SH(enable_$1))" = "yes")
    fi
])

dnl GP_CHECK_PLUGIN_DEPS(PluginName, VARIABLE-PREFIX,  modules...)
dnl Checks whether modules exist using PKG_CHECK_MODULES, and enables/disables
dnl plugins appropriately if enable_$plugin=auto
AC_DEFUN([GP_CHECK_PLUGIN_DEPS],
[
    if test "$m4_tolower(AS_TR_SH(enable_$1))" = "yes"; then
        PKG_CHECK_MODULES([$2], [$3])
    elif test "$m4_tolower(AS_TR_SH(enable_$1))" = "auto"; then
        PKG_CHECK_MODULES([$2], [$3],
                          [m4_tolower(AS_TR_SH(enable_$1))=yes],
                          [m4_tolower(AS_TR_SH(enable_$1))=no])
    fi

    AM_CONDITIONAL(m4_toupper(AS_TR_SH(ENABLE_$1)),
                   test "$m4_tolower(AS_TR_SH(enable_$1))" = yes)
])
