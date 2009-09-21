AC_DEFUN([GP_I18N],
[
    GETTEXT_PACKAGE=geany-plugins
    AC_SUBST(GETTEXT_PACKAGE)
    AC_DEFINE_UNQUOTED(
        [GETTEXT_PACKAGE],
        ["$GETTEXT_PACKAGE"],
        [The domain to use with gettext])
    LOCALEDIR="${datadir}/locale"
    AC_SUBST(LOCALEDIR)
    AM_GLIB_GNU_GETTEXT
])
