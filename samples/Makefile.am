

SAMPLES_SRC = \
	nocipher_proxy.c \
	socket.c \
	redirect_proxy.c

SAMPLES = $(SAMPLES_SRC:.c=)


sample_DATA = \
	README \
	$(SAMPLES_SRC)


sampledir = $(datadir)/doc/@PACKAGE@-@VERSION@/samples


EXTRA_DIST = $(sample_DATA)


$(SAMPLES):	%: %.c
	$(CC) $< -o $@ $(GLIB_CFLAGS) $(GLIB_LIBS) `PKG_CONFIG_PATH="$(prefix)/lib/pkgconfig" $(PKG_CONFIG) --cflags --libs meanwhile`


clean-local:
	rm -f $(SAMPLES)

