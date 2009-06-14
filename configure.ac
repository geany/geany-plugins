AC_PREREQ([2.63])
AC_INIT([geany-plugins], [0.17])
AM_INIT_AUTOMAKE([foreign dist-bzip2 tar-ustar subdir-objects])

AC_CONFIG_SRCDIR([po/LINGUAS])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([build/m4/cache])

AC_PROG_CC
AC_PROG_LIBTOOL

dnl i18n
IT_PROG_INTLTOOL([0.35.0])
GETTEXT_PACKAGE=geany-plugins
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(
    [GETTEXT_PACKAGE],
    ["$GETTEXT_PACKAGE"],
    [The domain to use with gettext])
AM_GLIB_GNU_GETTEXT
LOCALEDIR="${datadir}/locale"
AC_SUBST(LOCALEDIR)

dnl common checks
PKG_CHECK_MODULES([GEANY], [geany >= 0.16])
geanypluginsdir=`${PKG_CONFIG} --variable=libdir geany`/geany
AC_SUBST([geanypluginsdir])

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4], 
        [AM_CONDITIONAL(UNITTESTS, true)],
        [AM_CONDITIONAL(UNITTESTS, false)])

dnl plugin checks
GP_CHECK_ADDONS
GP_CHECK_GEANYGDB
GP_CHECK_GEANYLATEX
GP_CHECK_GEANYLIPSUM
GP_CHECK_GEANYSENDMAIL
GP_CHECK_SHIFTCOLUMN
GP_CHECK_SPELLCHECK

AC_CONFIG_FILES([
    Makefile
    po/Makefile.in
])
AC_OUTPUT
