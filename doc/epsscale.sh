#!/bin/sh

function boundingBoxScale() {
	BB=`echo "$1" | sed "s/%%BoundingBox\: //"`
	for I in $BB ; do
		J=`echo "$I * $2" | bc -qi`
		echo -n " $J"
	done
	echo
}

BB=`grep "^%%BoundingBox.*$" $1`
epsffit `boundingBoxScale "$BB" "$2"` "$1"

