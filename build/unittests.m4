AC_DEFUN([GP_CHECK_UNITTESTS],
[
    PKG_CHECK_MODULES([CHECK], [check >= $1],
            [AM_CONDITIONAL(UNITTESTS, true)],
            [AM_CONDITIONAL(UNITTESTS, false)])
])

