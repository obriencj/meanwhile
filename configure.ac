
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


# either auto-detect or specify the libgmp.a file for static linking
GMP_LIBA=
GMP_LIBS=
withval="auto"
AC_ARG_WITH(static_gmp,
	[  --with-static-gmp[=auto]  libgmp.a file for static linking])

if test "$withval" = "auto" ; then
	if test -f "/usr/lib/libgmp.a" ; then
	   GMP_LIBA="/usr/lib/libgmp.a"
	elif test -f "/usr/local/lib/libgmp.a" ; then
	   GMP_LIBA="/usr/local/lib/libgmp.a"
	fi

elif test -f "$withval" ; then
	   GMP_LIBA="$withval"

elif test -n "$withval" ; then
	   AC_MSG_ERROR([file not found, $withval])
fi

# if we've got a .a file, then we offer the option to link statically
AC_ARG_ENABLE(static,
	[  --enable-static-gmp      statically link in GMP],,
	enable_static="no")

if test $enable_static = yes ; then
   GMP_LIBS=$GMP_LIBA

else
   # GNU multiple precision arithmetic library. Automatically adds -lgmp
   # to LIBS
   AC_CHECK_LIB(gmp, __gmpz_init, ,
	[AC_MSG_ERROR([GNU MP not found, see http://swox.com/gmp])])
fi

AC_SUBST(GMP_LIBS)


AC_CONFIG_FILES(
	[Makefile src/Makefile]
	[samples/Makefile]
	[doc/Makefile doc/Doxyfile]
	[meanwhile.spec meanwhile.pc]
)

AC_OUTPUT()


echo
echo -n "GMP linking mode...       : "
if test "$enable_static" != "yes" ; then
   echo "dynamic"
else
   echo "static"
   echo "GMP library...            : $GMP_LIBA"
fi

echo
echo configure complete, now run \`make\`
echo


# The End.
