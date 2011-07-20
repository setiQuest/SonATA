#!/bin/tcsh

# lab-runpacketsend-data-capture-replay.tcsh

sudo packetsend -J 10.1.51.63 -j 50100 -f $1 -n 1 -i 100 -b 1 -P X 
