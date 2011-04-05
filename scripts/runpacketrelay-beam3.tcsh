#!/bin/tcsh

setenv thisDate `date  +'%Y-%m-%d-%H:%M'`
sudo packetrelay -f data-capture-beam3-${thisDate}.pktdata \
	-I 229.3.1.1 -i 53100 -n 5000000 
