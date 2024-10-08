AC_DEFUN([GP_CHECK_MARKDOWN],
[
    GP_ARG_DISABLE([markdown], [auto])

    AS_IF([test "x$enable_markdown" != "xno"], [
        PKG_CHECK_MODULES([CMARK], 
                          [libcmark], 
                          [md_enable_builtin_cmark=no], 
                          [md_enable_builtin_cmark=yes])
    ])

    AM_CONDITIONAL([MD_BUILTIN_CMARK], [test x$md_enable_builtin_cmark = xyes])

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
        markdown/cmark/Makefile
        markdown/docs/Makefile
    ])
])
