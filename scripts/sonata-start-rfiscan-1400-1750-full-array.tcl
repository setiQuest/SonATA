
# 2 beam
# sonata-start-rfiscan-1400-1750-full-array-obs.tcl

# send out "taking the array" email
sh echo "SonATA taking array" | mailx -s 'SonATA taking array for Rfi Scan' -r jjordan@seti.org jjordan@seti.org

# connect to telescope array
tscope setup

# allow some setup time
sh sleep 2

sched set beam1 on
sched set beam2 off
sched set beam3 on
sched set tasks autoselectants,prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq

sched set beginfreq 1400.0 
sched set endfreq 1750.0
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe on
sched set followup on
sched set catshigh spacecraft,habcat
sched set catslow tycho2subset,tycho2remainder
sched set multitarget off

db set host sse100
db set name sonatadb
db set usedb on

tscope set antlistsource antgroup
tscope set antsprimary antgroup
tscope set antsxpol antgroup
tscope set antsypol antgroup
 tscope assign beamxc1 antgroup
 tscope assign beamyc1 antgroup
 tscope assign beamxd1 antgroup
 tscope assign beamyd1 antgroup
 tscope assign beamxd2 antgroup
 tscope assign beamyd2 antgroup


act set type rfiscan


dx set length 94
dx set datareqsubchan 178
dx set baseinitaccum 20
dx set basewarn on
dx set baseerror on

# start it all up

#start tasks
