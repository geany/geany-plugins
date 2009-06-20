AC_DEFUN([GP_CHECK_GEANYLUA],
[
    AC_ARG_WITH([lua-pkg],
        AC_HELP_STRING([--with-lua-pkg=ARG],
            [name of Lua pkg-config script [[default=lua5.1]]]),
        [LUA_PKG_NAME=${withval%.pc}],
        [LUA_PKG_NAME=lua5.1

        for L in lua5.1 lua51 lua-5.1 lua; do
            PKG_CHECK_EXISTS([$L],
                [LUA_PKG_NAME=$L],[])
        done])

    PKG_CHECK_MODULES(LUA, [${LUA_PKG_NAME} >= 5.1])

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
