dnl add plugin status message, e.g. GP_STATUS_PLUGIN_ADD(plugin,yes)
AC_DEFUN([GP_STATUS_PLUGIN_ADD],
[
    plugins_statusmsg+="$1:$2
"
])

dnl add feature status message, e.g. GP_STATUS_FEATURE_ADD(feature,yes)
AC_DEFUN([GP_STATUS_FEATURE_ADD],
[
    feature_statusmsg+="$1:$2
"
])

dnl indent $1_statusmsg with RHS at col $2
AC_DEFUN([_GP_STATUS_PRINT_INDENT_],
[
    while read line; do
        test -z "$line" && break;
        plugin="    ${line%:*}:"
        status=${line#*:}
        let extracols=$2-${#plugin}
        echo -n "$plugin"
        for (( i=0; $i<$extracols; i++ )); do
            echo -n ' '
        done
        echo $status
    done << GPEOF
$$1_statusmsg
GPEOF
])

dnl print status message
AC_DEFUN([GP_STATUS_PRINT],
[
    GP_EXPAND_DIR(datadir)
    GP_EXPAND_DIR(libdir)
    GP_EXPAND_DIR(docdir)

    cat <<GPEOF

${PACKAGE}-${VERSION}

  Build Environment:
    Geany version:                ${GEANY_VERSION}
    Install prefix:               ${prefix}
    Datadir:                      ${expanded_datadir}/${PACKAGE_TARNAME}
    Libdir:                       ${expanded_libdir}/${PACKAGE_TARNAME}
    Docdir:                       ${expanded_docdir}
    Plugins path:                 ${geanypluginsdir}

  Plugins:
GPEOF

    _GP_STATUS_PRINT_INDENT_(plugins, 34)
    echo
    echo "  Features:"
    _GP_STATUS_PRINT_INDENT_(feature, 34)
    echo
])
