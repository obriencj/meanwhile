

SUBDIRS = mpi . python


noinst_PROGRAMS = test

test_SOURCES = test.c

test_CFLAGS = $(libmeanwhile_la_CFLAGS)

test_LDADD = libmeanwhile.la $(libmeanwhile_la_LIBADD)


lib_LTLIBRARIES = libmeanwhile.la


mwinclude_HEADERS = \
	mw_channel.h \
	mw_common.h \
	mw_encrypt.h \
	mw_error.h \
	mw_giosession.h \
	mw_message.h \
	mw_mpi.h \
	mw_object.h \
	mw_parser.h \
	mw_queue.h \
	mw_service.h \
	mw_session.h \
	mw_srvc_im.h \
	mw_srvc_store.h \
	mw_typedef.h


noinst_HEADERS = \
	mw_config.h \
	mw_debug.h \
	mw_marshal.h \
	mw_util.h


mwincludedir = $(includedir)/meanwhile


libmeanwhile_la_SOURCES = \
	channel.c \
	common.c \
	encrypt.c \
	error.c \
	giosession.c \
	message.c \
	mpi.c \
	object.c \
	parser.c \
	queue.c \
	service.c \
	session.c \
	srvc_im.c \
	srvc_store.c \
	mw_debug.c \
	mw_marshal.c \
	mw_util.c


mw_marshal.c:	marshal.list
	glib-genmarshal --body --prefix="mw_marshal" marshal.list > $@


mw_marshal.h:	marshal.list
	glib-genmarshal --header --prefix="mw_marshal"  marshal.list > $@


marshal:	mw_marshal.c mw_marshal.h


test.c:
	ln -s ../samples/sendmessage.c $@


libmeanwhile_la_CFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) $(GOBJECT_CFLAGS)


libmeanwhile_la_LIBADD = \
	mpi/libmpi.la \
	$(GLIB_LIBS) $(GOBJECT_LIBS)


libmeanwhile_la_LDFLAGS = \
	@MW_SO_OS_FLAGS@ \
	-version-info @MW_SO_VERSION@


AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"@PACKAGE@\"


# The end.
