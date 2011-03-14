dnl _GP_CHECK_CFLAG_(FLAG, [ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl Checks whether the C compiler understands FLAG
AC_DEFUN([_GP_CHECK_CFLAG_],
[
    gp_check_cflag_CFLAGS="$CFLAGS"
    CFLAGS="$1"
    AC_LANG([C])
    AC_MSG_CHECKING([whether the C compiler understands $CFLAGS])
    AC_COMPILE_IFELSE([
int main(void) {
    return 0;
}
],
                      [AC_MSG_RESULT([yes])
                       $2],
                      [AC_MSG_RESULT([no])
                       $3])
    CFLAGS="$gp_check_cflag_CFLAGS"
])

dnl GP_CHECK_CFLAGS
dnl Checks for default Geany-Plugis CFLAGS and defines GP_CFLAGS
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
            _GP_CHECK_CFLAG_([$flag], [AS_VAR_APPEND([GP_CFLAGS], [" $flag"])])
        done
    ])
    AC_SUBST([GP_CFLAGS])
    GP_STATUS_BUILD_FEATURE_ADD([Extra C compiler warnings],
                                [$enable_extra_c_warnings])
])
