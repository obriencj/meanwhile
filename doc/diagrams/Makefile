
all:
	echo "use the name of the diagram you'd like to convert/scale as"
	echo "the Makefile target"

%.eps: %.dia
	dia -n -t eps-pango -e $@.tmp $<
	./epsscale.sh $@.tmp 0.50 > $@
	rm -f $@.tmp

