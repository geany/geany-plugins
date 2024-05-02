dnl _CHECK_LIBMARKDOWN([action-if-found], [action-if-not-found])
dnl Searches for libmarkdown and define HAVE_MKDIO_H, LIBMARKDOWN_LIBS and
dnl LIBMARKDOWN_CFLAGS
AC_DEFUN([_CHECK_LIBMARKDOWN],
[
    old_LIBS=$LIBS
    LIBS=
    LIBMARKDOWN_LIBS=
    LIBMARKDOWN_CFLAGS=
    AC_SEARCH_LIBS([mkd_compile], [markdown],
                   [AC_CHECK_HEADERS([mkdio.h],
                                     [LIBMARKDOWN_LIBS=$LIBS
                                      LIBMARKDOWN_CFLAGS=
                                      $1],
                                     [$2])],
                   [$2])
    AC_SUBST([LIBMARKDOWN_CFLAGS])
    AC_SUBST([LIBMARKDOWN_LIBS])
    LIBS=$old_LIBS
])

AC_DEFUN([GP_CHECK_MARKDOWN],
[
    GP_ARG_DISABLE([markdown], [auto])
    AC_ARG_ENABLE([peg-markdown],
                  [AS_HELP_STRING([--enable-peg-markdown],
                                  [Whether to use Peg-Markdown library [[default=auto]]])],
                  [enable_peg_markdown=$enableval],
                  [enable_peg_markdown=auto])

    dnl check which markdown library to use
    AS_IF([test "x$enable_markdown" != xno &&
           test "x$enable_peg_markdown" != xyes],
          [_CHECK_LIBMARKDOWN([enable_peg_markdown=no],
                              [AS_IF([test "x$enable_peg_markdown" != xno],
                                     [enable_peg_markdown=yes],
                                     [test "x$enable_markdown" = xyes],
                                     [AC_MSG_ERROR([libmarkdown not found])],
                                     [enable_markdown=no
                                      AC_MSG_WARN([libmarkdown not found, disabling Markdown plugin])])])])
    AM_CONDITIONAL([MARKDOWN_PEG_MARKDOWN],
                   [test "x$enable_peg_markdown" = xyes])
    dnl fancy status
    AS_IF([test "x$enable_peg_markdown" = xyes],
          [markdown_library=peg-markdown],
          [markdown_library=libmarkdown])
    GP_STATUS_FEATURE_ADD([Markdown library], [$markdown_library])

    GTK_VERSION=3.0
    WEBKIT_VERSION=2.30

    dnl Support both webkit2gtk 4.0 and 4.1, as the only difference is the
    dnl libsoup version in the API, which we don't use.
    dnl Prefer the 4.1 version, but use the 4.0 version as fallback if
    dnl available -- yet still ask for the 4.1 if neither are available
    webkit_package=webkit2gtk-4.1
    PKG_CHECK_EXISTS([${webkit_package} >= ${WEBKIT_VERSION}],,
                     [PKG_CHECK_EXISTS([webkit2gtk-4.0 >= ${WEBKIT_VERSION}],
                                       [webkit_package=webkit2gtk-4.0])])

    GP_CHECK_PLUGIN_DEPS([markdown], [MARKDOWN],
                         [$GP_GTK_PACKAGE >= ${GTK_VERSION}
                          $webkit_package >= ${WEBKIT_VERSION}
                          gthread-2.0])

    GP_COMMIT_PLUGIN_STATUS([Markdown])

    AC_CONFIG_FILES([
        markdown/Makefile
        markdown/src/Makefile
        markdown/docs/Makefile
        markdown/peg-markdown/Makefile
        markdown/peg-markdown/peg-0.1.9/Makefile
    ])
])
