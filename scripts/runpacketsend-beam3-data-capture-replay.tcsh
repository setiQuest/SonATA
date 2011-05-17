#!/bin/tcsh

# runpacketsend-beam3-data-capture-replay.tcsh

# seti002-1

sudo packetsend -J 10.1.50.59 -j 50300 -f $1 -n 1 -i 300 -b 1 -P X 
