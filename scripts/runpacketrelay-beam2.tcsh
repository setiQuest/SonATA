#!/bin/tcsh

setenv thisDate `date  +'%Y-%m-%d-%H:%M'`
sudo packetrelay -f data-capture-beam2-${thisDate}.pktdata \
	-I 229.2.1.1 -i 52100 -n 5000000 
