
SUBDIRS = . @WITH_PYTHON_SUBDIR@

lib_LTLIBRARIES = libmeanwhile.la

mwinclude_HEADERS = \
	mw_channel.h \
	mw_cipher.h \
	mw_common.h \
	mw_error.h \
	mw_message.h \
	mw_service.h \
	mw_session.h \
	mw_srvc_aware.h \
	mw_srvc_conf.h \
	mw_srvc_im.h \
	mw_srvc_store.h \
	mw_st_list.h

noinst_HEADERS = \
	mw_compat.h \
	mw_debug.h

mwincludedir = $(includedir)/meanwhile

libmeanwhile_la_CFLAGS = \
	-Wall -Werror $(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS)

libmeanwhile_la_LDFLAGS = -no-undefined

libmeanwhile_la_SOURCES = \
	channel.c \
	cipher.c \
	common.c \
	error.c \
	message.c \
	mw_debug.c \
	service.c \
	session.c \
	srvc_aware.c \
	srvc_conf.c \
	srvc_im.c \
	srvc_store.c \
	st_list.c

libmeanwhile_la_LIBADD = $(GLIB_LIBS)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"@PACKAGE@\"

