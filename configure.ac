
# version of meanwhile
m4_define(meanwhile_major,	1)
m4_define(meanwhile_minor,	0)
m4_define(meanwhile_micro,	2)

# release of meanwhile. 0 for CVS, 1 for release
m4_define(meanwhile_release,	1)

# required and compat version of glib2.
m4_define(glib_required_version,	2.0.0)



AC_INIT
AM_INIT_AUTOMAKE(meanwhile, meanwhile_major.meanwhile_minor.meanwhile_micro)

RELEASE=meanwhile_release
AC_SUBST(RELEASE)

AC_PREREQ([2.50])

AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"

AC_HEADER_STDC



# current:revision:age
MW_SO_VERSION=$((meanwhile_major+meanwhile_minor)):meanwhile_micro:meanwhile_minor
AC_SUBST(MW_SO_VERSION)



# os-specific so flags
case "${host}" in
*darwin)
	MW_SO_OS_FLAGS="-dynamic-lib";;
*mingw32 | *cygwin)
	MW_SO_OS_FLAGS="-shared -no-undefined";;
*)
	MW_SO_OS_FLAGS="-shared";;
esac
AC_SUBST(MW_SO_OS_FLAGS)



# Debugging option
AC_ARG_ENABLE(debug,
	[  --enable-debug	  compile with debugging support],
	AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.]), )



# debugging output urging people to send mail
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



# libm for use in mpi.c
AC_CHECK_LIB(m, main, LIBM="-lm")
AC_CHECK_HEADER(math.h)
AC_SUBST(LIBM)



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

echo -n "mailme debug mode........ : "
if test "$MW_MAILME" = 1 ; then
   echo "enabled"
else
   echo "disabled"
fi

echo -n "Doxygen generation....... : "
if test "$enable_doxygen" = "yes" ; then
   echo "enabled"
else
   echo "disabled"
fi

echo
echo configure complete, now run \`make\`
echo
echo you may need to run \`ldconfig\` as root after installation before
echo being able to load this library
echo

# The End.
