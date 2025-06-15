AC_DEFUN([GP_CHECK_GEANYLUA],
[
    GP_ARG_DISABLE([GeanyLua], [auto])

    AC_ARG_WITH([lua-pkg],
        AS_HELP_STRING([--with-lua-pkg=ARG],
            [name of Lua pkg-config script [[default=lua5.1]]]),
        [LUA_PKG_NAME=${withval%.pc}],
        [LUA_PKG_NAME=lua5.1

        for L in lua5.1 lua51 lua-5.1 lua; do
            PKG_CHECK_EXISTS([$L],
                [LUA_PKG_NAME=$L]; break,[])
        done])

    LUA_VERSION=5.1
    LUA_VERSION_BOUNDARY=5.2
    GP_CHECK_PLUGIN_DEPS([GeanyLua], [LUA],
                         [${LUA_PKG_NAME} >= ${LUA_VERSION}
                          ${LUA_PKG_NAME} < ${LUA_VERSION_BOUNDARY}])
    GP_CHECK_PLUGIN_DEPS([GeanyLua], [GMODULE], [gmodule-2.0])
    GP_COMMIT_PLUGIN_STATUS([GeanyLua])

    dnl AC_CONFIG_FILES was removed from here. It is now handled by the main configure.ac.
])
