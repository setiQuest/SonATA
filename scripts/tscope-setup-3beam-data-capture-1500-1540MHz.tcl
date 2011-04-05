# tscope-setup-3beam-data-capture-1500-1540MHz.tcl


sched set beginfreq 1500.0 
sched set endfreq 1540.0
sched set dxtune range
sched set rftune auto
sched set target user
sched set multitarget on
sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set tasks autoselectants,prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq


dx set length 256
dx set datareqsubchan 384
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off

db set name sonatadb
db set usedb on

tscope set antlistsource {antgroup} current
tscope set antsprimary {antgroup} current
tscope set antsxpol {antgroup} current
tscope set antsypol {antgroup} current
tscope set centertuneoffset 0.000000000 current
tscope set tuneoffset 0.000000000 current

