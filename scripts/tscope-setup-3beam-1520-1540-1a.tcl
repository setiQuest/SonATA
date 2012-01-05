# 3 beams

# tscope-setup-3beam-1520-1540-1a.tcl

sh echo "SonATA taking array" | mailx -s 'SonATA taking array for Testing' -r jjordan@seti.org ata-staff@seti.org

act set targetbeam1 170
act set targetbeam2 170
act set targetbeam3 170
act set targetprimary 170
act set type target
act set candarch confirmed
act set multitargetnulls off

sched set beginfreq 1520.0 
sched set endfreq 1540.1
sched set dxtune range
sched set rftune auto
sched set target user
sched set multitarget on
sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay



db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  1a
tscope set antsxpol 1a
tscope set antsypol 1a
# both tunings must be the same
tscope set tuningc 1530.0
tscope set tuningd 1530.0
 tscope assign beamxc1 1a
 tscope assign beamyc1 1a
 tscope assign beamxd1 1a
 tscope assign beamyd1 1a
 tscope assign beamxd2 1a
 tscope assign beamyd2 1a

