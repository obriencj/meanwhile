

TEX_SRC =	meanwhile.tex

DVI_OUT =	$(TEX_SRC:%.tex=%.dvi)


DIA_SRC =	chan_create.dia \
		chan_create_proxy.dia

EPS_OUT =	$(DIA_SRC:%.dia=%.eps)


all:	pdf html


pdf:	$(EPS_OUT) $(DVI_OUT)
	dvipdf $(DVI_OUT)


html:	$(EPS_OUT)
	latex2html $(TEX_SRC) > /dev/null


%.dvi: %.tex
	latex -interaction=batchmode $< > /dev/null
	latex -interaction=batchmode $< > /dev/null


%.eps: %.dia
	dia -n -t eps-pango -e $@.tmp $<
	./epsscale.sh $@.tmp 0.50 > $@
	rm -f $@.tmp


clean:
	rm -rf meanwhile/
	rm -f .nfs* *.aux *.dvi *.eps *.lof *.log *.out *.pdf *.toc

