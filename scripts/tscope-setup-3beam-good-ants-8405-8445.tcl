# 3 beams
# tscope-setup-3beam-good-ants-8405-8445.tcl
sh echo " SonATA taking array" | mailx -s 'SonATA taking array for Rosetta Test' -r jjordan@seti.org ata-staff@seti.org

tscope setup

act set targetbeam1 160
act set targetbeam2 160
act set targetbeam3 160
act set targetprimary 160
act set type target
act set candarch confirmed
act set multitargetnulls off

sched set beginfreq 8405.0 
sched set endfreq 8445.0
sched set dxtune range
sched set rftune auto
sched set target user
sched set multitarget on
sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq


dx set length 93
dx set datareqsubchan 384
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off

db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope set antsxpol 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope set antsypol 1a,1c,1d,2a,2e,5b,5c,5e
# both tunings must be the same
tscope set tuningc 8423.0
tscope set tuningd 8423.0
 tscope assign beamxc1 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
 tscope assign beamyc1 1a,1c,1d,2a,2e,5b,5c,5e
 tscope assign beamxd1 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
 tscope assign beamyd1 1a,1c,1d,2a,2e,5b,5c,5e
 tscope assign beamxd2 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
 tscope assign beamyd2 1a,1c,1d,2a,2e,5b,5c,5e

