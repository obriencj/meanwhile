
# version of meanwhile
m4_define(meanwhile_version,		0.4.0)

# required and compat version of glib2.
m4_define(glib_required_version,	2.0.0)

# this is just to generate the requirement in the specfile. It would
# be useful to make it check the available python version here as
# well...
m4_define(python_required_version,	2.2)


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


dnl Python wrapper option
withval="auto"
AC_ARG_WITH(python,
	[  --with-python[=auto]   compile Python wrappers for Meanwhile])

if test "$withval" != "no" ; then

	dnl Check 1: Python binary
	if test "$withval" = "auto" ; then
		AC_PATH_PROG(pythonpath, python)
		if test "_$pythonpath" = _ ; then
			AC_MSG_RESULT(not found, building without python support)
			withval="no"
		fi
	else
		pythonpath="$withval"
	fi

	PYTHON="$pythonpath"
	AC_SUBST(PYTHON)
fi

if test "$withval" != "no" ; then

	dnl Check 2: Python version
	AC_MSG_CHECKING(Python version)
	PYTHON_FOUND_VERSION=`$pythonpath -c 'import distutils.sysconfig; print "%s" % distutils.sysconfig.get_config_var("VERSION")' 2>/dev/null`

	if test "$PYTHON_FOUND_VERSION" = "" ; then
		AC_MSG_ERROR([unable to determine version])
	else

		# TODO: put in a check against PYTHON_FOUND_VERSION and
		# python_required_version to make sure we meet the minimum
		# requirements

		AC_MSG_RESULT($PYTHON_FOUND_VERSION)
		PYTHON_VERSION=python_required_version
		AC_SUBST(PYTHON_VERSION)
	fi

	dnl Check 3: Python CFLAGS
	AC_MSG_CHECKING(for Python compile flags)
	PYTHON_CFLAGS=`$pythonpath -c 'import distutils.sysconfig; print "-I%s" % distutils.sysconfig.get_python_inc()' 2>/dev/null`

	if test "$PYTHON_CFLAGS" = "" ; then
		AC_MSG_ERROR(not found)
	else
		AC_MSG_RESULT($PYTHON_CFLAGS)
		AC_SUBST(PYTHON_CFLAGS)
	fi


	dnl Check 4: Python LIBS
	AC_MSG_CHECKING(for Python linker flags)
	PYTHON_LIBP=`$pythonpath -c 'import distutils.sysconfig; print "%s" % distutils.sysconfig.get_config_var("LIBP")' 2>/dev/null`

	PYTHON_LIBS=`$pythonpath -c 'import distutils.sysconfig; print "-L%s %s" % (distutils.sysconfig.get_config_var("LIBP"), distutils.sysconfig.get_config_var("LIBS"))' 2>/dev/null`

	if test "$PYTHON_LIBS" = "" ; then
		AC_MSG_ERROR(not found)
	else
		AC_MSG_RESULT($PYTHON_LIBS)
		AC_SUBST(PYTHON_LIBS)
		AC_SUBST(PYTHON_LIBP)
	fi

	WITH_PYTHON_SUBPACKAGE=1
	AC_SUBST(WITH_PYTHON_SUBPACKAGE)

	WITH_PYTHON_SUBDIR="python"
	AC_SUBST(WITH_PYTHON_SUBDIR)
fi


dnl Glib-2.0 option with some version detection for compat.h
PKG_CHECK_MODULES(GLIB, glib-2.0,
[
	AC_DEFINE(HAVE_GLIB, 1, [Define if we've found glib-2.0.])
])

# AM_PATH_GLIB_2_0(glib_required_version, ,
# 	AC_MSG_ERROR([*** GLib glib_required_version is required.])
# )
GLIB_VERSION=glib_required_version
AC_SUBST(GLIB_VERSION)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


AC_OUTPUT([Makefile src/Makefile src/python/Makefile meanwhile.spec meanwhile.pc doc/Doxyfile test.sh])

chmod +x test.sh

