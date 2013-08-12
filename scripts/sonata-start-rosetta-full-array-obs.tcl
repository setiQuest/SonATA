
# 3 beam
# sonata-start-rosetta-full-array-obs.tcl

# send out "taking the array" email
#sh echo "SonATA taking array" | mailx -s 'SonATA taking array for Rosetta Test' -r jjordan@seti.org jjordan@seti.org

# connect to telescope array
tscope setup

# allow some setup time
sh sleep 2

sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set tasks autoselectants,prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq,obs

# 24 dxs 800 KHz
sched set beginfreq 8418.0 
sched set endfreq 8438.1
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe on
sched set followup on
sched set catshigh spacecraft,habcat
sched set catslow tycho2subset,tycho2remainder
sched set multitarget on

db set host mozart
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


act set targetbeam1 160
act set targetbeam2 160
act set targetbeam3 160
act set targetprimary 160
act set type target
act set candarch confirmed
act set delay 20


dx set length 94
dx set datareqsubchan 178
dx set baseinitaccum 10
dx set basewarn off
dx set baseerror off

# start it all up

start tasks
