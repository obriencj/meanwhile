

pylibdir = /usr/lib/python@WITH_PYTHON_VERSION@
pyincdir = /usr/include/python@WITH_PYTHON_VERSION@
pythondir = ${pyincdir}/site-packages

PYTHON_CFLAGS = -I ${pyincdir}


python_PROGRAMS = _meanwhile.so

python_SCRIPTS = meanwhile.py

noinst_HEADERS = py_meanwhile.h

_meanwhile_so_SOURCES = \
	py_meanwhile.c \
	py_channel.c \
	py_service.c \
	py_session.c \
	py_srvc_im.c

_meanwhile_so_CFLAGS = \
	-Wall -Werror \
	$(GLIB_CFLAGS) $(PYTHON_CFLAGS)

_meanwhile_so_LDFLAGS = \
	-no-undefined \
	-shared -Wl,-soname,_meanwhile.so

_meanwhile_so_LDADD = \
	../.libs/libmeanwhile.so \
	$(GLIB_LIBS)

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"@PACKAGE@-python\"

