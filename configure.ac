
AC_INIT
AM_INIT_AUTOMAKE(meanwhile, 0.3)

AC_PREREQ([2.50])

AC_PROG_CC
AC_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"
AC_PROG_INSTALL

AC_HEADER_STDC
AC_HEADER_SYS_WAIT

AC_C_CONST
AC_STRUCT_TM

AC_ARG_ENABLE(debug, [  --enable-debug	  compile with debugging support],,enable_debug=no)
if test "$enable_debug" = yes; then
   AC_DEFINE(DEBUG, 1, [Define if debugging is enabled.])
fi

CFLAGS="$CFLAGS -Wall -Werror"
AC_SUBST(CFLAGS)

AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR([
*** GLib 2.0 is required.]))

AC_SUBST(GLIB_CFLAGS)

AC_OUTPUT([Makefile src/Makefile meanwhile.spec])

