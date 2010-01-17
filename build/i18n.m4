AC_DEFUN([GP_I18N],
[
    if test -n "${LINGUAS}"
    then
        ALL_LINGUAS="${LINGUAS}"
    else
        if test -z "$conf_dir" ; then
            conf_dir="."
        fi
        ALL_LINGUAS=`cd "$conf_dir/po" 2>/dev/null && ls *.po 2>/dev/null | $AWK 'BEGIN { FS="."; ORS=" " } { print $[1] }'`
    fi
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
