
# version of meanwhile
m4_define(meanwhile_version,		0.4.0)

# required and compat version of glib2.
m4_define(glib_required_version,	2.0.0)


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


dnl Darwin...
if test `uname` = "Darwin" ; then
	os_so_ext=".dylib"
	os_ld_flags="-dynamic-lib"
else	
	os_so_ext=".so"
	os_ld_flags="-shared -no-undefined"
fi
AC_SUBST(os_so_ext)
AC_SUBST(os_ld_flags)


dnl Glib-2.0 option with some version detection for compat.h
AM_PATH_GLIB_2_0(glib_required_version, ,
	AC_MSG_ERROR([*** GLib glib_required_version is required.])
 )
GLIB_VERSION=glib_required_version
AC_SUBST(GLIB_VERSION)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


AC_CONFIG_FILES(
	[Makefile src/Makefile]
	[doc/Doxyfile]
	[meanwhile.spec meanwhile.pc]
)

AC_OUTPUT()
