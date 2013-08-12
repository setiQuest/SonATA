#!/bin/tcsh

# runpacketsend-dx-vger-xpol.tcsh

sudo packetsend -c -f ${HOME}/sonata_install/data/vger-xpol-2010-07-14-406.pktdata -J 127.0.0.1 -j 51100 -n 1 -i 1050 -b 1
