
# version of meanwhile
m4_define(meanwhile_version,		0.5.0)

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



GMP_LIBA=
GMP_LIBS=
GMP_CFLAGS=

# either auto-detect or specify the libgmp.a file for static linking
withval="auto"
AC_ARG_WITH(gmp-prefix,
	[  --with-gmp-prefix[=auto]  prefix gmp was installed to])

if test "$withval" = "auto" ; then
	found_prefix=no

	if test $found_prefix = "no" ; then
	   try_prefix=/usr
	   AC_CHECK_HEADER([$try_prefix/include/gmp.h], found_prefix="yes")
	fi

	if test $found_prefix = "no" ; then
	   try_prefix=/usr/local
	   AC_CHECK_HEADER([$try_prefix/include/gmp.h], found_prefix="yes")
	fi

	if test $found_prefix = "no" ; then
	   AC_MSG_ERROR([GNU MP headers not found])
	fi

elif test -n "$withval" ; then
	try_prefix=$withval

	AC_CHECK_HEADER([$try_prefix/include/gmp.h],,
		[AC_MSG_ERROR([GNU MP headers not found])])

else
	AC_MSG_ERROR([GNU MP headers not found, $withval])
fi

GMP_CFLAGS=-I$try_prefix/include
GMP_LIBA=$try_prefix/lib/libgmp.a

# if we're linking statically, then GMP_LIBS needs to be GMP_LIBA,
# else we need to ensure we can find the library for dynamic linking
AC_ARG_ENABLE(static-gmp,
	[  --enable-static-gmp     statically link in GMP],
	[static_gmp="yes"])

if test "$static_gmp" = "yes" ; then
   GMP_LIBS=$GMP_LIBA
else
   LD_LIBRARY_PATH="$try_prefix/lib":"$LD_LIBRARY_PATH"
   export LD_LIBRARY_PATH

   AC_CHECK_LIB(gmp, __gmpz_init,
	[GMP_LIBS="-L$try_prefix/lib -lgmp"],
	[AC_MSG_ERROR([GNU MP libraries not found, see http://swox.com/gmp])])
fi

AC_SUBST(GMP_CFLAGS)
AC_SUBST(GMP_LIBS)



AC_CONFIG_FILES(
	[Makefile src/Makefile]
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

echo -n "GMP linking mode...       : "
if test "$static_gmp" = "yes" ; then
   echo "static"
   echo "GMP library...            : $GMP_LIBA"
else
   echo "dynamic"
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
