
# version of meanwhile
m4_define(meanwhile_version,		0.4.0)

# required and compat version of glib2. This is a hack so that a build
# against 2.0.0 (which will use the compat.h hacks to function) will
# create package requirements for only 2.0.0, while builds against
# 2.2.0 or later will require 2.2.0

m4_define(glib_required_version,	2.0.0)
m4_define(glib_compat_version,		2.2.0)
m4_define(glib_compat_major_version,	2)
m4_define(glib_compat_minor_version,	2)

m4_define(python_required_version,	2.3)


AC_INIT
AM_INIT_AUTOMAKE(meanwhile, meanwhile_version)

RELEASE=1
AC_SUBST(RELEASE)

AC_PREREQ([2.50])

AC_PROG_CC

AC_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"

AC_PROG_INSTALL

AC_HEADER_STDC
AC_HEADER_SYS_WAIT

AC_C_CONST
AC_STRUCT_TM


dnl Debugging option
AC_ARG_ENABLE(debug,
	[  --enable-debug	  compile with debugging support],
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.]), )


dnl Auto-detect which python bindings should be built.
dnl lifted with extreme prejudice from RPM
dnl there's a better way (I think) to do this
withval=auto
AC_ARG_WITH(python,
	[  --with-python           build meanwhile python bindings ])

WITH_PYTHON_VERSION=$withval
if test $withval = auto ; then
	AC_MSG_CHECKING(for python 2.3)

	AC_RUN_IFELSE([AC_LANG_SOURCE([[
		#include <python2.3/Python.h>
			main() {
			  exit(strncmp("2.3", PY_VERSION, 3));
			} ]])],[withval=yes],[withval=no],[withval=yes])
	AC_MSG_RESULT($withval)

	if test $withval = yes ; then
		WITH_PYTHON_VERSION="2.3"
	else
		AC_MSG_CHECKING(for python 2.2)
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
			#include <python2.2/Python.h>
			  main() {
			    exit(strncmp("2.2", PY_VERSION, 3));
			  } ]])],[withval=yes],[withval=no],[withval=yes])
		AC_MSG_RESULT($withval)

		if test $withval = yes ; then
			WITH_PYTHON_VERSION="2.2"
		else
			WITH_PYTHON_VERSION=no
		fi
	fi
fi

if test "$WITH_PYTHON_VERSION" != no ; then
	WITH_PYTHON_SUBDIR=python
	WITH_PYTHON_SUBPACKAGE=1
	PYTHON_LIBS="$LIBDIR/python$WITH_PYTHON_VERSION"
else
	WITH_PYTHON_SUBDIR=
	WITH_PYTHON_SUBPACKAGE=0
fi
AC_SUBST(WITH_PYTHON_SUBDIR)
AC_SUBST(WITH_PYTHON_SUBPACKAGE)
AC_SUBST(WITH_PYTHON_VERSION)
AC_SUBST(PYTHON_LIBS)



dnl Glib-2.0 option with some version detection for compat.h
AM_PATH_GLIB_2_0(glib_required_version, ,
	AC_MSG_ERROR([*** GLib glib_required_version is required.])
)

if ((test $glib_config_major_version -ge glib_compat_major_version) &&
    (test $glib_config_minor_version -ge glib_compat_minor_version))
then
	GLIB_VERSION=glib_compat_version
else
	GLIB_VERSION=glib_required_version
fi
AC_SUBST(GLIB_VERSION)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


AC_OUTPUT([Makefile src/Makefile src/python/Makefile meanwhile.spec meanwhile.pc])

