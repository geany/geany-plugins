AC_DEFUN([GP_CHECK_GEANYPRJ],
[
    GP_ARG_DISABLE([GeanyPrj], [auto])
    
    GP_CHECK_PLUGIN_GTK2_ONLY([GeanyPrj])
    # _GNU_SOURCE strcasestr is useful for a better search experience
    # in sidebar which lists project files.
    AC_CHECK_FUNCS(strcasestr)
    
    GP_COMMIT_PLUGIN_STATUS([GeanyPrj])
    AC_CONFIG_FILES([
        geanyprj/Makefile
        geanyprj/src/Makefile
    ])
])
