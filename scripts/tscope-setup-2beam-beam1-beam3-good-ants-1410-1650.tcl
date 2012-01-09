# 2 beams
# tscope-setup-2beam-beam1-beam3-good-ants-1410-1650.tcl

sh echo " SonATA taking array" | mailx -s 'SonATA taking bf1, bf3 and array for Testing' -r jjordan@seti.org ata-staff@seti.org



sched set beginfreq 1410.0 
sched set endfreq 1650.0
sched set beam1 on
sched set beam2 off
sched set beam3 on
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq



db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope set antsxpol 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope set antsypol 1a,1c,1d,2a,2e,5b,5c,5e
# both tunings must be the same
tscope set tuningc 8420.0
tscope set tuningd 8420.0
 tscope assign beamxc1 1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
 tscope assign beamyc1 1c,1d,2a,2e,5b,5c,5e
 #tscope assign beamxd1 1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
 #tscope assign beamyd1 1c,1d,2a,2e,5b,5c,5e
 tscope assign beamxd2 1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
 tscope assign beamyd2 1c,1d,2a,2e,5b,5c,5e

