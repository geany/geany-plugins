AC_DEFUN([GP_CHECK_GEANYVOSK],
[
    GP_ARG_DISABLE([Geanyvosk], [auto])

    AS_CASE([$host_os],
            [cygwin* | mingw* | win32*],
            [],

            [GP_CHECK_PLUGIN_DEPS([geanyvosk], [ALSA], [alsa])

             AC_CHECK_HEADER([vosk_api.h],
               [AC_CHECK_LIB([vosk], [vosk_model_new],
                 [VOSK_LIBS="-lvosk"
                  AC_DEFINE([HAVE_VOSK], [1], [Vosk ASR available])],
                 [AC_MSG_WARN([libvosk not found; GeanyVosk will load but ASR disabled])])],
               [AC_MSG_WARN([vosk_api.h not found; GeanyVosk will load but ASR disabled])])

             AC_SUBST([VOSK_CFLAGS])
             AC_SUBST([VOSK_LIBS])])

    GP_COMMIT_PLUGIN_STATUS([Geanyvosk])

    AC_CONFIG_FILES([
        geanyvosk/Makefile
        geanyvosk/src/Makefile
    ])
])
