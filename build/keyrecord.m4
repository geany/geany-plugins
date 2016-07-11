AC_DEFUN([GP_CHECK_KEYRECORD],
 [
     GP_ARG_DISABLE([keyrecord], [auto])
     GP_COMMIT_PLUGIN_STATUS([keyrecord])
     AC_CONFIG_FILES([
         keyrecord/Makefile
         keyrecord/src/Makefile
     ])
 ])
