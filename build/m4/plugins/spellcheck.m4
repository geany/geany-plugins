AC_DEFUN([GP_CHECK_SPELLCHECK],
[
    PKG_CHECK_MODULES(ENCHANT, [enchant >= 0.13])
    AC_CONFIG_FILES([
        spellcheck/Makefile
        spellcheck/src/Makefile
    ])
])
