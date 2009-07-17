AC_DEFUN([_GP_EXPAND_PREFIX_],
[
    case $prefix in
        NONE) prefix=$ac_default_prefix ;;
        *) ;;
    esac

    case $exec_prefix in
        NONE) exec_prefix=$prefix ;;
        *) ;;
    esac
])

AC_DEFUN([GP_EXPAND_DIR],
[
    AC_REQUIRE([_GP_EXPAND_PREFIX_])

    expanded_$1=$(eval echo $$1)
    expanded_$1=$(eval echo $expanded_$1)
])
