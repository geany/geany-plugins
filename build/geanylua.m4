AC_DEFUN([GP_CHECK_GEANYLUA],
[
    GP_ARG_DISABLE([GeanyLua], [auto])

    AC_ARG_WITH([lua-pkg],
        AC_HELP_STRING([--with-lua-pkg=ARG],
            [name of Lua pkg-config script [[default=lua5.1]]]),
        [LUA_PKG_NAME=${withval%.pc}],
        [LUA_PKG_NAME=lua5.1

        for L in lua5.1 lua51 lua-5.1 lua; do
            PKG_CHECK_EXISTS([$L],
                [LUA_PKG_NAME=$L]; break,[])
        done])

    LUA_VERSION=5.1
    GP_CHECK_PLUGIN_DEPS([GeanyLua], [LUA],
                         [${LUA_PKG_NAME} >= ${LUA_VERSION}])
    GP_CHECK_PLUGIN_DEPS([GeanyLua], [GMODULE], [gmodule-2.0])
    GP_COMMIT_PLUGIN_STATUS([GeanyLua])

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
