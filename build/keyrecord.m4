AC_DEFUN([GP_CHECK_KEYRECORD],
 [
     GP_ARG_DISABLE([Keyrecord], [auto])
     GP_COMMIT_PLUGIN_STATUS([Keyrecord])
     AC_CONFIG_FILES([
         keyrecord/Makefile
         keyrecord/src/Makefile
     ])
 ])
