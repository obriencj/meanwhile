

AM_CFLAGS = -I../src/ $(GLIB_CFLAGS)
AM_LDADD = $(GLIB_LIBS) ../src/libmeanwhile.la

noinst_PROGRAMS = \
	nocipher_proxy \
	redirect_proxy \
	socket

nocipher_proxy_SOURCES = nocipher_proxy.c
nocipher_proxy_LDADD = $(AM_LDADD)

socket_SOURCES = socket.c
socket_LDADD = $(AM_LDADD)

redirect_proxy_SOURCES = redirect_proxy.c
redirect_proxy_LDADD = $(AM_LDADD)

