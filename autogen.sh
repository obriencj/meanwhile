#! /bin/sh


# just to help us differentiate messages
function ECHO() {
	echo "[AUTOGEN]" $*
}



# OSX has glibtoolize, everywhere else is just libtoolize
if test -z "${LTIZE}" ; then
	ECHO "Trying to find a libtoolize"

	if sh -c "libtoolize --version" 2>&1 1> /dev/null ; then
		LTIZE=libtoolize
	elif sh -c "glibtoolize --version" 2>&1 1> /dev/null ; then
		LTIZE=glibtoolize
	fi
fi
if test -z "${LTIZE}" ; then
	ECHO "Couldn't figure out a libtoolize to use. Specify one with LTIZE"
else
	ECHO "Running $LTIZE"
	$LTIZE || exit $?
fi



# let's make sure we can find our aclocal macros
if test -d /usr/local/share/aclocal ; then
	ACLOCAL_FLAGS="-I /usr/local/share/aclocal"
fi

ECHO "Running aclocal $ACLOCAL_FLAGS"
aclocal $ACLOCAL_FLAGS || exit $?



# ECHO "Running autoheader"
# autoheader || exit $?



# put in license and stuff if necessary
if test -z "$AUTOMAKE_FLAGS" ; then
	AUTOMAKE_FLAGS="--add-missing --copy"
fi

ECHO "Running automake $AUTOMAKE_FLAGS"
automake $AUTOMAKE_FLAGS



ECHO "Running autoconf"
autoconf || exit $?



ECHO "Running automake"
automake || exit $?
if test `uname` = "Darwin" ; then
	automake Makefile 2> /dev/null
fi



ECHO "Running ./configure $@"
./configure $@


ECHO "Done"
# The End
