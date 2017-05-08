#! /bin/bash

cat $1 | gawk '
BEGIN {FS=","; cross=0; internal=0}
/HHA/ {if (cross!=0) {print cross;print internal; printf("%.2f%\n", (100*cross/(internal+cross))) } print; cross=0; internal=0;next}
/SLLC/ {print cross;print internal; printf("%.2f%\n", (100*cross/(internal+cross))) ; exit}
{ cross+= $7; internal+=$8}'
