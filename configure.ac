
# version of meanwhile
m4_define(meanwhile_version,		0.4.3cvs)

# release of meanwhile
m4_define(meanwhile_release,		0)

# required and compat version of glib2.
m4_define(glib_required_version,	2.0.0)


AC_INIT
AM_INIT_AUTOMAKE(meanwhile, meanwhile_version)

RELEASE=meanwhile_release
AC_SUBST(RELEASE)

AC_PREREQ([2.50])

AM_MAINTAINER_MODE

AC_PROG_CC

AC_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"

AC_PROG_INSTALL

AC_HEADER_STDC
AC_HEADER_SYS_WAIT

AC_C_CONST
AC_STRUCT_TM


# Debugging option
AC_ARG_ENABLE(debug,
	[  --enable-debug	  compile with debugging support],
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.]), )


# Darwin...
if test `uname` = "Darwin" ; then
	os_so_ext=".dylib"
	os_ld_flags="-dynamic-lib"
else	
	os_so_ext=".so"
	os_ld_flags="-shared -no-undefined"
fi
AC_SUBST(os_so_ext)
AC_SUBST(os_ld_flags)


# Glib-2.0
PKG_CHECK_MODULES(GLIB,
[glib-2.0 >= glib_required_version],
[
	AC_DEFINE(HAVE_GLIB, 1, [Define if we've found glib.])
])

GLIB_VERSION=glib_required_version
AC_SUBST(GLIB_VERSION)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


# GMP
AC_CHECK_LIB(gmp, __gmpz_init, ,
	[AC_MSG_ERROR([GNU MP not found, see http://swox.com/gmp])])


AC_CONFIG_FILES(
	[Makefile src/Makefile]
	[samples/Makefile]
	[doc/Makefile doc/Doxyfile]
	[meanwhile.spec meanwhile.pc]
)

AC_OUTPUT()

