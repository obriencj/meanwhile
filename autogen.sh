#! /bin/sh

libtoolize && \
aclocal && \
# autoheader && \
automake --add-missing --copy &&
autoconf &&
automake &&
echo "Now run ./configure"
# ./configure

