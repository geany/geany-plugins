AC_DEFUN([GP_CHECK_SPELLCHECK],
[
    PKG_CHECK_MODULES(ENCHANT, [enchant >= 1.3])
    AC_CONFIG_FILES([
        spellcheck/Makefile
        spellcheck/src/Makefile
    ])
])
