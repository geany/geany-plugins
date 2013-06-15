# Copyright (c) 2011 Colomban Wendling <ban@herbesfolles.org>
#
# AX_PYTHON_LIBRARY([action-if-found], [action-if-not-found])
#
# Tries to locate Python DSO path. It requires $PYTHON to be set

AC_DEFUN([AX_PYTHON_LIBRARY],[
	AC_REQUIRE([AX_PYTHON_DEVEL])

	AC_MSG_CHECKING([for Python DSO location])

	ax_python_library=`cat << EOD | $PYTHON - 2>/dev/null
from distutils.sysconfig import get_config_vars
from os.path import join as path_join

cvars = get_config_vars()
# support multiarch-enabled distributions like Ubuntu
if not 'MULTIARCH' in cvars.keys():
    cvars[['MULTIARCH']] = ''
print(path_join(cvars[['LIBDIR']], cvars[['MULTIARCH']], cvars[['LDLIBRARY']]))
EOD`

	AC_SUBST([PYTHON_LIBRARY], [$ax_python_library])
	AC_MSG_RESULT([$PYTHON_LIBRARY])
	AS_IF([test -z "$ax_python_library"], [$2], [$1])
])
