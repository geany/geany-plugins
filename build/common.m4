dnl GP_ARG_DISABLE(PluginName, default)
dnl - default can either be yes(enabled) or no(disabled), or auto(to be used
dnl   with GP_CHECK_PLUGIN_DEPS)
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
])

dnl GP_COMMIT_PLUGIN_STATUS(PluginName)
dnl Commits the enabled status of a plugin
dnl This macro must be called once for each plugin after all other GP macros.
AC_DEFUN([GP_COMMIT_PLUGIN_STATUS],
[
    dnl if choice wasn't made yet, enable it
    if test "$m4_tolower(AS_TR_SH(enable_$1))" = "auto"; then
        m4_tolower(AS_TR_SH(enable_$1))=yes
    fi
    AM_CONDITIONAL(m4_toupper(AS_TR_SH(ENABLE_$1)),
                   test "$m4_tolower(AS_TR_SH(enable_$1))" = yes)
    GP_STATUS_PLUGIN_ADD([$1], [$m4_tolower(AS_TR_SH(enable_$1))])
])
