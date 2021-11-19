dnl _GP_CHECK_CFLAG(FLAG, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl Checks whether the C compiler works when passed FLAG
dnl This might not mean it actually understand the flag, in case it is
dnl forgiving about unknown flags.
AC_DEFUN([_GP_CHECK_CFLAG],
[
    gp_check_cflag_CFLAGS="$CFLAGS"
    CFLAGS="$1"
    AC_LANG_PUSH(C)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main(void) {return 0;}])],
                      [$2], [$3])
    AC_LANG_POP(C)
    CFLAGS="$gp_check_cflag_CFLAGS"
])

dnl _GP_CHECK_CFLAG_WERROR
dnl Checks for the flag the compiler has to treat warnings as errors
dnl Sets $_GP_CFLAG_WERROR
AC_DEFUN([_GP_CHECK_CFLAG_WERROR],
[
    _GP_CFLAG_WERROR=
    AC_MSG_CHECKING([for the C compiler flag to treat warnings as errors])
    _GP_CHECK_CFLAG([-Werror], [_GP_CFLAG_WERROR="-Werror"])
    AC_MSG_RESULT([${_GP_CFLAG_WERROR:-none}])
])

dnl GP_CHECK_CFLAG(FLAG, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl Checks whether the C compiler understands FLAG
AC_DEFUN([GP_CHECK_CFLAG],
[
    AC_REQUIRE([_GP_CHECK_CFLAG_WERROR])

    AC_MSG_CHECKING([whether the C compiler understands $1])
    _GP_CHECK_CFLAG([$1 $_GP_CFLAG_WERROR],
                    [AC_MSG_RESULT([yes])
                     $2],
                    [AC_MSG_RESULT([no])
                     $3])
])

dnl GP_CHECK_CFLAGS
dnl Checks for default Geany-Plugins CFLAGS and defines GP_CFLAGS
AC_DEFUN([GP_CHECK_CFLAGS],
[
    AC_ARG_ENABLE([extra-c-warnings],
                  AS_HELP_STRING([--disable-extra-c-warnings],
                                 [Disable extra C Compiler warnings]),
                  [enable_extra_c_warnings=$enableval],
                  [enable_extra_c_warnings=yes])

    GP_CFLAGS=
    AS_IF([test "x$enable_extra_c_warnings" != xno],
    [
        enable_extra_c_warnings=yes
        for flag in -Wall \
                    -Wimplicit-function-declaration \
                    -Wmissing-parameter-type \
                    -Wold-style-declaration \
                    -Wpointer-arith \
                    -Wshadow \
                    -Wundef \
                    -Wwrite-strings
        do
            GP_CHECK_CFLAG([$flag], [GP_CFLAGS="${GP_CFLAGS} $flag"])
        done
    ])
    AC_SUBST([GP_CFLAGS])
    GP_STATUS_BUILD_FEATURE_ADD([Extra C compiler warnings],
                                [$enable_extra_c_warnings])
])
