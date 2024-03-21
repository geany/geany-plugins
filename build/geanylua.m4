AC_DEFUN([GP_CHECK_GEANYLUA],
[
    GP_ARG_DISABLE([GeanyLua], [auto])

    AC_ARG_WITH([lua-pkg],
        AC_HELP_STRING([--with-lua-pkg=ARG],
            [name of Lua pkg-config script [[default=lua]]]),
        [LUA_PKG_NAME=${withval%.pc}],
        [LUA_PKG_NAME=lua

        for L in "$LUA_PKG_NAME" lua lua53 lua54 lua52 luajit lua51; do
            PKG_CHECK_EXISTS([$L],
                [LUA_PKG_NAME=$L]; break,[])
        done])

    if [[ "x$LUA_PKG_NAME" = "xluajit" ]] ; then
        LUA_VERSION=2.0
        LUA_VERSION_BOUNDARY=3.0
    elif [[ "x$LUA_PKG_NAME" = "xlua51" ]] ; then
        LUA_VERSION=5.1
        LUA_VERSION_BOUNDARY=5.2
    elif [[ "x$LUA_PKG_NAME" = "xlua52" ]] ; then
        LUA_VERSION=5.2
        LUA_VERSION_BOUNDARY=5.3
    elif [[ "x$LUA_PKG_NAME" = "xlua53" ]] ; then
        LUA_VERSION=5.3
        LUA_VERSION_BOUNDARY=5.4
    elif [[ "x$LUA_PKG_NAME" = "xlua54" ]] ; then
        LUA_VERSION=5.4
        LUA_VERSION_BOUNDARY=5.5
    else
        LUA_VERSION=5.1
        LUA_VERSION_BOUNDARY=5.5
    fi

    GP_CHECK_PLUGIN_DEPS([GeanyLua], [LUA],
                         [${LUA_PKG_NAME} >= ${LUA_VERSION}
                          ${LUA_PKG_NAME} < ${LUA_VERSION_BOUNDARY}])
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
