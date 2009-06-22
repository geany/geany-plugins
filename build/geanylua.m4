AC_DEFUN([GP_CHECK_GEANYLUA],
[
    AC_ARG_ENABLE(geanylua,
        AC_HELP_STRING([--enable-geanylua=ARG],
            [Enable GeanyLua plugin [[default=auto]]]),,
        [enable_geanylua=auto])

    AC_ARG_WITH([lua-pkg],
        AC_HELP_STRING([--with-lua-pkg=ARG],
            [name of Lua pkg-config script [[default=lua5.1]]]),
        [LUA_PKG_NAME=${withval%.pc}
        enable_geanylua=yes],
        [LUA_PKG_NAME=lua5.1

        for L in lua5.1 lua51 lua-5.1 lua; do
            PKG_CHECK_EXISTS([$L],
                [LUA_PKG_NAME=$L],[])
        done])

    LUA_VERSION=5.1
    if [[ x"$enable_geanylua" = "xauto" ]]; then
        PKG_CHECK_MODULES(LUA, [${LUA_PKG_NAME} >= ${LUA_VERSION}],
            [enable_geanylua=yes],
            [enable_geanylua=no])
    elif [[ x"$enable_geanylua" = "xyes" ]]; then
        PKG_CHECK_MODULES(LUA, [${LUA_PKG_NAME} >= ${LUA_VERSION}])
    fi

    AM_CONDITIONAL(ENABLE_GEANYLUA, test $enable_geanylua = yes)

    AC_CONFIG_FILES([
        geanylua/examples/edit/Makefile
        geanylua/examples/scripting/Makefile
        geanylua/examples/info/Makefile
        geanylua/examples/work/Makefile
        geanylua/examples/dialogs/Makefile
        geanylua/examples/Makefile
        geanylua/docs/Makefile
        geanylua/Makefile
    ])
])
