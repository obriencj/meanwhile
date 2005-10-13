

SAMPLES_SRC = \
	logging_proxy.c \
	login_server.c \
	nocipher_proxy.c \
	sendmessage.c \
	socket.c \
	redirect_proxy.c

SAMPLES = $(SAMPLES_SRC:.c=)


sample_SCRIPTS = \
	build

sample_DATA = \
	README \
	$(SAMPLES_SRC)


sampledir = $(datadir)/doc/@PACKAGE@-@VERSION@/samples


EXTRA_DIST = $(sample_DATA) $(sample_SCRIPTS)


$(SAMPLES):	%: %.c
	$(CC) $< -o $@ $(GLIB_CFLAGS) $(GLIB_LIBS) `PKG_CONFIG_PATH="$(prefix)/lib/pkgconfig" $(PKG_CONFIG) --cflags --libs meanwhile`


clean-local:
	rm -f $(SAMPLES)

