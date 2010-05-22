# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([geanygendoc],
        [0.1],
        [ban@herbesfolles.org],
        [geanygendoc])
AC_CONFIG_SRCDIR([src/ggd.c])
AC_CONFIG_AUX_DIR([build/aux])
AC_CONFIG_MACRO_DIR([build/m4])
AM_INIT_AUTOMAKE([1.11.1 -Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
LT_PREREQ([2.2.0])
LT_INIT([disable-static])
AC_PROG_INSTALL

AC_PATH_PROG([RST2HTML], [rst2html], [no])
AC_SUBST([RST2HTML])
BUILD_RST=`test "x$RST2HTML" != "xno" && echo Yes || echo No`
AM_CONDITIONAL([BUILD_RST], [test "x$BUILD_RST" == "xYes"])

# checking for Geany
PKG_CHECK_MODULES(GEANY,  [geany      >= 0.18.1])
PKG_CHECK_MODULES(GTK,    [gtk+-2.0   >= 2.12])
PKG_CHECK_MODULES(GLIB,   [glib-2.0   >= 2.14])
# need GIO >= 2.18 for g_memory_output_stream_get_data_size()
PKG_CHECK_MODULES(GIO,    [gio-2.0 >= 2.18])
PKG_CHECK_MODULES(CTPL,   [ctpl    >= 0.2])

GEANY_VERSION=`$PKG_CONFIG --modversion geany`
GTK_VERSION=`$PKG_CONFIG --modversion gtk+-2.0`
GLIB_VERSION=`$PKG_CONFIG --modversion glib-2.0`
GIO_VERSION=`$PKG_CONFIG --modversion gio-2.0`
CTPL_VERSION=`$PKG_CONFIG --modversion ctpl`

# i18n
GETTEXT_PACKAGE=geanygendoc
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package.])

ALL_LINGUAS="`sed -e '/^#/d' $srcdir/po/LINGUAS`" # take all languages found in file po/LINGUAS

AM_GLIB_GNU_GETTEXT
# workaround for intltool bug (http://bugzilla.gnome.org/show_bug.cgi?id=490845)
if test "x$MSGFMT" = "xno"; then
	AC_MSG_ERROR([msgfmt not found. Please install the gettext package.])
fi

# intltool hack to define install_sh on Debian/Ubuntu systems
if test "x$install_sh" = "x"; then
	install_sh="`pwd`/install-sh"
	AC_SUBST(install_sh)
fi

# get the plugin installed at the correct location for Geany
# TODO find a way to NOT override --libdir command line option if given
libdir="`$PKG_CONFIG --variable=libdir geany`/geany"
# same for the data, since we use geany->app->datadir
plugindatadir="`$PKG_CONFIG --variable=datadir geany`/geany/plugins/${PACKAGE_TARNAME}"
AC_SUBST([plugindatadir])
# docdir
docdir="`$PKG_CONFIG --variable=prefix geany`/share/doc/geany-plugins/${PACKAGE_TARNAME}/"


AC_CONFIG_FILES([
  README
  Makefile
  src/Makefile
  data/Makefile
  data/filetypes/Makefile
  po/Makefile.in
  docs/Makefile
  docs/help/Makefile
])
AC_OUTPUT

echo "----------------------------------------"
echo "Using Geany version                  : ${GEANY_VERSION}"
echo "Using GLib version                   : ${GLIB_VERSION}"
echo "Using GIO version                    : ${GIO_VERSION}"
echo "Using GTK version                    : ${GTK_VERSION}"
echo "Using CTPL version                   : ${CTPL_VERSION}"
echo "Build documentation                  : $BUILD_RST"
echo ""
echo "Configuration is done OK."
echo ""
