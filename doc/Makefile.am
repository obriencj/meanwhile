
noinst_DATA = Doxyfile


if ENABLE_DOXYGEN

htmldoc_DATA = html/*
htmldocdir = $(datadir)/doc/@PACKAGE@-doc-@VERSION@/html

latexdoc_DATA = latex/*
latexdocdir = $(datadir)/doc/@PACKAGE@-doc-@VERSION@/latex

docdata = $(htmldoc_DATA) $(latexdoc_DATA)

$(docdata): doxygen

else

install:

endif


doxygen:
	@doxygen Doxyfile > /dev/null


clean-local:
	rm -rf html latex man

