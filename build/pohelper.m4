AC_DEFUN([GP_CHECK_POHELPER],
[
    GP_ARG_DISABLE([PoHelper], [auto])

    dnl We require GLib >= 2.22, and Geany only depends on 2.20
    GP_CHECK_PLUGIN_DEPS([PoHelper], [GLIB], [glib-2.0 >= 2.22])

    GP_COMMIT_PLUGIN_STATUS([PoHelper])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
