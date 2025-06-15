AC_DEFUN([GP_CHECK_LATEX],
[
    GP_ARG_DISABLE([LaTeX], [auto])
    GP_COMMIT_PLUGIN_STATUS([LaTeX])
    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
