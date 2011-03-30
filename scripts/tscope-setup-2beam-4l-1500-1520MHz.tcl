#  antenna 4L
# tscope-setup-2beam-4l-1500-1520MHz.tcl


sched set beginfreq 1500.0 
sched set endfreq 1520.0
sched set dxtune range
sched set rftune auto
sched set multitarget on
sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay


db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  4l
tscope set antsxpol 4l
tscope set antsypol 4l
# both tunings must be the same
tscope set tuningc 1500.0
tscope set tuningd 1520.0
 tscope assign beamxc1 4l
 tscope assign beamyc1 4l
 tscope assign beamxd1 4l
 tscope assign beamyd1 4l
 tscope assign beamxd2 4l
 tscope assign beamyd2 4l

