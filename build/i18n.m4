AC_DEFUN([GP_I18N],
[
    GETTEXT_PACKAGE=geany-plugins
    AC_SUBST(GETTEXT_PACKAGE)
    AC_DEFINE_UNQUOTED(
    o    [GETTEXT_PACKAGE],
        ["$GETTEXT_PACKAGE"],
        [The domain to use with gettext])
    LOCALEDIR="${datadir}/locale"
    AC_SUBST(LOCALEDIR)

    AM_GNU_GETTEXT_VERSION([0.19.8])
    AM_GNU_GETTEXT([external])
])
