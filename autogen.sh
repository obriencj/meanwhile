#! /bin/sh


# just to help us differentiate messages
ECHO() {
    echo "[AUTOGEN]" $*
}


quiet_test() {
    sh -c "$*" >/dev/null 2>&1
    return $?
}


# OSX has glibtoolize, everywhere else is just libtoolize
if test -z "${LTIZE}" ; then
    ECHO "Trying to find a libtoolize"
    
    if quiet_test "libtoolize --version" ; then
	LTIZE=libtoolize
    elif quiet_test "glibtoolize --version" ; then
	LTIZE=glibtoolize
    fi
fi
if test -z "${LTIZE}" ; then
    ECHO "Couldn't figure out a libtoolize to use. Specify one with LTIZE"
else
    ECHO "Running $LTIZE --force"
    $LTIZE --force || exit $?
fi



# let's make sure we can find our aclocal macros
if test -d /usr/local/share/aclocal ; then
    ACLOCAL_FLAGS="-I /usr/local/share/aclocal"
fi

ECHO "Running aclocal $ACLOCAL_FLAGS"
ECHO "(please ignore any non-fatal errors)"
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
automake Makefile 2> /dev/null



ECHO "Running ./configure $@"
./configure $@



ECHO "Done"
# The End
