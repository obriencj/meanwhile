

lib_LTLIBRARIES = libmeanwhile.la

mwinclude_HEADERS = \
	meanwhile.h \
	channel.h \
	cipher.h \
	common.h \
	error.h \
	message.h \
	service.h \
	session.h \
	srvc_aware.h \
	srvc_conf.h \
	srvc_im.h \
	srvc_store.h \
	st_list.h

mwincludedir = $(includedir)/meanwhile

libmeanwhile_la_CFLAGS = -DG_LOG_DOMAIN=\"@PACKAGE@\"

libmeanwhile_la_LIBADD = $(GLIB_LIBS)
libmeanwhile_la_LDFLAGS= -no-undefined
libmeanwhile_la_SOURCES = \
	channel.c \
	cipher.c \
	common.c \
	error.c \
	message.c \
	service.c \
	session.c \
	srvc_aware.c \
	srvc_conf.c \
	srvc_im.c \
	srvc_store.c \
	st_list.c \
	mw_debug.c \
	mw_debug.h

INCLUDES = $(GLIB_CFLAGS)

