AC_DEFUN([GP_CHECK_GEANYGDB],
[
    GP_ARG_DISABLE([GeanyGDB], [yes])
    GP_STATUS_PLUGIN_ADD([GeanyGDB], [$enable_geanygdb])
    AC_CHECK_HEADERS([elf.h])
    AC_CHECK_HEADERS([elf_abi.h])
    AC_CONFIG_FILES([
        geanygdb/Makefile
        geanygdb/src/Makefile
        geanygdb/tests/Makefile
    ])
])
