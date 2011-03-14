AC_DEFUN([GP_CHECK_UNITTESTS],
[
    gp_have_unittests=no
    PKG_CHECK_MODULES([CHECK], [check >= $1],
            [AM_CONDITIONAL(UNITTESTS, true)
             gp_have_unittests=yes],
            [AM_CONDITIONAL(UNITTESTS, false)])
    GP_STATUS_BUILD_FEATURE_ADD([Unit tests], [$gp_have_unittests])
])

