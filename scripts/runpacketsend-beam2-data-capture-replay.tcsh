#!/bin/tcsh

# runpacketsend-beam2-data-capture-replay.tcsh

# seti001-1

echo "sudo packetsend -J 10.1.50.55 -j 50200 -f $1 -n 1 -i 300 -b 1 -P X "
sudo packetsend -J 10.1.50.55 -j 50200 -f $1 -n 1 -i 300 -b 1 -P X 
