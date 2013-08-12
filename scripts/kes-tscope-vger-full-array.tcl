# 3 beam
# kes-tscope-vger-full-array.tcl

# send out "taking the array" email
#sh echo "SonATA taking array" | mailx -s 'SonATA taking array for Vger Test' -r kes@smolek.com kes@smolek.com

# connect to telescope array
tscope setup

# allow some setup time
sh sleep 2

sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set tasks autoselectants,prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq

# 24 dxs 800 KHz
sched set beginfreq 8410.0 
sched set endfreq 8430.0
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe on
sched set followup on
sched set catshigh spacecraft,habcat
sched set catslow tycho2subset,tycho2remainder
sched set multitarget on
sched set followup on

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
tscope set centertuneoffset 0.0
tscope set tuneoffset 0.0
tscope set tuningc 8430.0
tscope set tuningd 8430.0

act set targetbeam1 131
act set targetbeam2 131
act set targetbeam3 131
act set targetprimary 131
act set multitargetnulls off
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
