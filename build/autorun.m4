 AC_DEFUN([GP_CHECK_AUTORUN],
 [
     GP_ARG_DISABLE([autorun], [auto])
     GP_COMMIT_PLUGIN_STATUS([Autorun])
     AC_CONFIG_FILES([
         autorun/Makefile
         autorun/src/Makefile
     ])
 ])
