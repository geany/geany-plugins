dnl checks for the GTK version to build against (2 or 3)
dnl defines GP_GTK_PACKAGE (e.g. "gtk+-2.0"), GP_GTK_VERSION (e.g. "2.24") and
dnl GP_GTK_VERSION_MAJOR (e.g. "2")
AC_DEFUN([GP_CHECK_GTK_VERSION],
[
    AC_REQUIRE([AC_PROG_AWK])
    AC_REQUIRE([PKG_PROG_PKG_CONFIG])

    _gtk_req=$(${PKG_CONFIG} --print-requires geany | ${AWK} '/^gtk\+-/{print}')
    GP_GTK_PACKAGE=$(echo ${_gtk_req} | ${AWK} '{print $[]1}')
    GP_GTK_VERSION=$(echo ${_gtk_req} | ${AWK} '{print $[]3}')
    GP_GTK_VERSION_MAJOR=$(echo ${GP_GTK_VERSION} | cut -d. -f1)
    AC_SUBST([GP_GTK_PACKAGE])
    AC_SUBST([GP_GTK_VERSION])
    AC_SUBST([GP_GTK_VERSION_MAJOR])
])

dnl executes $1 if GTK3 is used, and $2 otherwise
AC_DEFUN([GP_CHECK_GTK3],
[
    AC_REQUIRE([GP_CHECK_GTK_VERSION])
    AS_IF([test ${GP_GTK_VERSION_MAJOR} = 3],[$1],[$2])
])
