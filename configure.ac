
# version of meanwhile
m4_define(meanwhile_version,		0.6.0)

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



# Debugging option
AC_ARG_ENABLE(debug,
	[  --enable-debug	  compile with debugging support],
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.]), )



enableval="yes"
AC_ARG_ENABLE(mailme,
	[  --enable-mailme[[=yes]]   enable mw_debug_mailme output], )

MW_MAILME=1
if test "$enableval" = "no" ; then
   MW_MAILME=0
fi
AC_SUBST(MW_MAILME)



# Doxygen generation option
enableval="yes"
AC_ARG_ENABLE(doxygen,
	[  --enable-doxygen[[=yes]]  enable the doc package], )

enable_doxygen=$enableval

AM_CONDITIONAL(ENABLE_DOXYGEN, test "$enable_doxygen" = "yes")


# doc sub package
# right now this depends only on doxygen
WITH_DOC_SUBPACKAGE=
if test "$enable_doxygen" = "yes" ; then
   WITH_DOC_SUBPACKAGE=1
fi
AC_SUBST(WITH_DOC_SUBPACKAGE)



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



AC_CONFIG_FILES(
	[Makefile src/Makefile src/mpi/Makefile]
	[samples/Makefile]
	[doc/Makefile doc/Doxyfile]
	[meanwhile.spec meanwhile.pc]
)

AC_CONFIG_FILES([samples/build], [chmod +x samples/build])


AC_OUTPUT()



echo

echo -n "mailme debug mode..       : "
if test "$MW_MAILME" = 1 ; then
   echo "enabled"
else
   echo "disabled"
fi

echo -n "Doxygen generation...     : "
if test "$enable_doxygen" = "yes" ; then
   echo "enabled"
else
   echo "disabled"
fi

echo
echo configure complete, now run \`make\`
echo


# The End.
