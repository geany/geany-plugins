dnl GP_CHECK_LDFLAG(FLAG, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl Checks whether the linker understands FLAG
AC_DEFUN([GP_CHECK_LDFLAG],
[
    gp_check_ldflag_LDFLAGS="$LDFLAGS"
    LDFLAGS="$1"
    AC_MSG_CHECKING([whether the linker understands $LDFLAGS])
    AC_LANG_PUSH(C)
    AC_LINK_IFELSE([AC_LANG_SOURCE([int main(void) {return 0;}])],
                   [AC_MSG_RESULT([yes])
                    $2],
                   [AC_MSG_RESULT([no])
                    $3])
    AC_LANG_POP(C)
    LDFLAGS="$gp_check_ldflag_LDFLAGS"
])

dnl GP_CHECK_LDFLAGS
dnl Checks for default Geany-Plugins LDFLAGS and defines GP_LDFLAGS
AC_DEFUN([GP_CHECK_LDFLAGS],
[
    AC_ARG_ENABLE([extra-ld-flags],
                  AS_HELP_STRING([--disable-extra-ld-flags],
                                 [Disable extra linker flags]),
                  [enable_extra_ld_flags=$enableval],
                  [enable_extra_ld_flags=yes])

    GP_LDFLAGS=
    AS_IF([test "x$enable_extra_ld_flags" != xno],
    [
        enable_extra_ld_flags=yes
        for flag in -Wl,-z,defs # do not allow undefined symbols in object files
        do
            GP_CHECK_LDFLAG([$flag], [GP_LDFLAGS="${GP_LDFLAGS} $flag"])
        done
    ])
    AC_SUBST([GP_LDFLAGS])
    GP_STATUS_BUILD_FEATURE_ADD([Extra linker options],
                                [$enable_extra_c_warnings])
])
