

MW_SRC =	meanwhile.tex
MW_OUT =	$(MW_SRC:%.tex=%.dvi)

HOURI_SRC =	draft-houri.tex
HOURI_OUT =	$(HOURI_SRC:%.tex=%.dvi)


all:	mw-pdf mw-html houri-pdf houri-html doxygen


mw-pdf:	$(MW_OUT)
	dvipdf $(MW_OUT)


mw-html:
	latex2html $(MW_SRC) > /dev/null


houri-pdf:	$(HOURI_OUT)
	dvipdf $(HOURI_OUT)


houri-html:
	latex2html $(HOURI_SRC) > /dev/null


%.dvi: %.tex
	latex -interaction=batchmode $<
	latex -interaction=batchmode $< > /dev/null


doxygen:
	mkdir doxygen
	doxygen doxygen.conf > /dev/null


clean:
	rm -rf meanwhile/ doxygen/
	rm -f *.nfs *.aux *.dvi *.lof *.log *.out *.pdf *.toc

