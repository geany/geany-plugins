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

    GTK_VERSION=2.16
    WEBKIT_VERSION=1.1.13

    GP_CHECK_GTK3([webkit_package=webkitgtk-3.0],
                  [webkit_package=webkit-1.0])

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
