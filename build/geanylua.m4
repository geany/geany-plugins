AC_DEFUN([GP_CHECK_GEANYLUA],
[
    GP_ARG_DISABLE([GeanyLua], [auto])

    AC_ARG_WITH([lua-pkg],
        AS_HELP_STRING([--with-lua-pkg=ARG],
            [name of Lua pkg-config script [[default=lua]]]),
        [LUA_PKG_NAME=${withval%.pc}],
        [LUA_PKG_NAME=lua])

    AS_CASE([$LUA_PKG_NAME],
        [luajit], [LUA_VERSION_MIN=2.0; LUA_VERSION_LIMIT=3.0],
        [*],      [LUA_VERSION_MIN=5.1; LUA_VERSION_LIMIT=5.5]
    )

    GP_CHECK_PLUGIN_DEPS([GeanyLua], [LUA],
                         [${LUA_PKG_NAME} >= ${LUA_VERSION_MIN}
                          ${LUA_PKG_NAME} < ${LUA_VERSION_LIMIT}])
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
