

AM_CFLAGS = -I../src/ $(GLIB_CFLAGS)
common_libs = $(GLIB_LIBS) ../src/.libs/libmeanwhile.a

noinst_PROGRAMS = \
	nocipher_proxy \
	redirect_proxy \
	socket

nocipher_proxy_SOURCES = nocipher_proxy.c
nocipher_proxy_LDADD = $(common_libs)

socket_SOURCES = socket.c
socket_LDADD = $(common_libs)

redirect_proxy_SOURCES = redirect_proxy.c
redirect_proxy_LDADD = $(common_libs)

