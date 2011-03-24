#!/bin/tcsh

# runpacketsend-data-capture-replay.tcsh

sudo packetsend -J 10.1.50.51 -j 50100 -f $1 -n 1 -i 300 -b 1 -P X 
