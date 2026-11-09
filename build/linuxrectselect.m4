AC_DEFUN([GP_CHECK_LINUXRECTSELECT],
[
    GP_ARG_DISABLE([LinuxRectSelect], [auto])
    GP_CHECK_PLUGIN_DEPS([LinuxRectSelect], [LINUXRECTSELECT],
                         [$GP_GTK_PACKAGE >= 2.8])
    GP_COMMIT_PLUGIN_STATUS([LinuxRectSelect])
    AC_CONFIG_FILES([
        linuxrectselect/Makefile
        linuxrectselect/src/Makefile
    ])
])
