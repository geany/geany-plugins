dnl checks for the GTK version to build against (2 or 3)
dnl defines GP_GTK_PACKAGE (e.g. "gtk+-2.0"), GP_GTK_VERSION (e.g. "2.24") and
dnl GP_GTK_VERSION_MAJOR (e.g. "2");  and defines the GP_GTK3 AM conditional
AC_DEFUN([GP_CHECK_GTK_VERSION],
[
    AC_REQUIRE([AC_PROG_AWK])
    AC_REQUIRE([AC_PROG_SED])
    AC_REQUIRE([PKG_PROG_PKG_CONFIG])

    GP_GEANY_PKG_CONFIG_PATH_PUSH

    _gtk_req=$(${PKG_CONFIG} --print-requires geany | ${AWK} '/^gtk\+-/{print}')
    GP_GTK_PACKAGE=$(echo ${_gtk_req} | ${SED} 's/ *[[><=]].*$//')
    GP_GTK_VERSION=$(echo ${_gtk_req} | ${SED} 's/^.*[[><=]] *//')
    GP_GTK_VERSION_MAJOR=$(echo ${GP_GTK_VERSION} | cut -d. -f1)
    AC_SUBST([GP_GTK_PACKAGE])
    AC_SUBST([GP_GTK_VERSION])
    AC_SUBST([GP_GTK_VERSION_MAJOR])

    AM_CONDITIONAL([GP_GTK3], [test "x$GP_GTK_VERSION_MAJOR" = x3])

    GP_GEANY_PKG_CONFIG_PATH_POP
])

dnl executes $1 if GTK3 is used, and $2 otherwise
AC_DEFUN([GP_CHECK_GTK3],
[
    AC_REQUIRE([GP_CHECK_GTK_VERSION])
    AS_IF([test ${GP_GTK_VERSION_MAJOR} = 3],[$1],[$2])
])

dnl GP_CHECK_PLUGIN_GTKN_ONLY pluginname gtkversion
AC_DEFUN([GP_CHECK_PLUGIN_GTKN_ONLY],
[
    AC_REQUIRE([GP_CHECK_GTK_VERSION])

    AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = no],[],
          [AC_MSG_CHECKING([whether the GTK version in use is compatible with plugin $1])
           AS_IF([test ${GP_GTK_VERSION_MAJOR} = $2],
                 [AC_MSG_RESULT([yes])],
                 [AC_MSG_RESULT([no])
                  AS_IF([test "$m4_tolower(AS_TR_SH(enable_$1))" = yes],
                        [AC_MSG_ERROR([$1 is not compatible with the GTK version in use])],
                        [test "$m4_tolower(AS_TR_SH(enable_$1))" = auto],
                        [m4_tolower(AS_TR_SH(enable_$1))=no])])])
])

dnl GP_CHECK_PLUGIN_GTK2_ONLY pluginname
AC_DEFUN([GP_CHECK_PLUGIN_GTK2_ONLY],
[
    GP_CHECK_PLUGIN_GTKN_ONLY([$1], [2])
])

dnl GP_CHECK_PLUGIN_GTK3_ONLY pluginname
AC_DEFUN([GP_CHECK_PLUGIN_GTK3_ONLY],
[
    GP_CHECK_PLUGIN_GTKN_ONLY([$1], [3])
])
