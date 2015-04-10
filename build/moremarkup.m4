AC_DEFUN([GP_CHECK_MOREMARKUP],
[
    GP_ARG_DISABLE([MoreMarkup], [auto])

    GP_COMMIT_PLUGIN_STATUS([MoreMarkup])

    AC_CONFIG_FILES([
        moremarkup/Makefile
	moremarkup/src/Makefile
    ])
])
