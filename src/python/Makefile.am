

pythondir = ${PYTHON_LIBP}/site-packages


python_PROGRAMS = _meanwhile.so

python_SCRIPTS = meanwhile.py

noinst_HEADERS = py_meanwhile.h

noinst_SCRIPTS = test.py

_meanwhile_so_SOURCES = \
	py_meanwhile.c \
	py_channel.c \
	py_service.c \
	py_session.c \
	py_srvc_aware.c \
	py_srvc_conf.c \
	py_srvc_im.c \
	py_srvc_resolve.c \
	py_srvc_store.c

_meanwhile_so_CFLAGS = \
	-Wall -Werror $(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) $(PYTHON_CFLAGS)

_meanwhile_so_LDFLAGS = \
	-no-undefined \
	-shared
#	-shared -Wl,-soname,_meanwhile.so

_meanwhile_so_LDADD = \
	../.libs/libmeanwhile.so \
	$(GLIB_LIBS) $(PYTHON_LIBS)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"@PACKAGE@-python\"

