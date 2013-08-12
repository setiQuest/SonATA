# 1 beam
# sonata-start-rosetta-4l.obs.tcl

# send out "taking the array" email
#sh echo " SonATA taking array" | mailx -s 'SonATA taking array for Rosetta Test' -r jjordan@seti.org jjordan@seti.org

# connect to telescope array
#tscope setup

# allow some setup time
sh sleep 2

sched set beam1 on
sched set beam2 off
sched set beam3 off
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay,obs

# 24 dxs 800 KHz
sched set beginfreq 8418.0 
sched set endfreq 8438.1
sched set dxtune range
sched set rftune auto
sched set target user
sched set beam1 on
sched set beam2 off
sched set beam3 off
sched set pipe on
sched set followup off

db set host mozart
db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  4l
tscope set antsxpol 4l
tscope set antsypol 4l
# both tunings must be the same
 tscope assign beamxc1 4l
 tscope assign beamyc1 4l
 tscope assign beamxd1 4l
 tscope assign beamyd1 4l
 tscope assign beamxd2 4l
 tscope assign beamyd2 4l

act set targetbeam1 160
act set targetprimary 160
act set type target
act set candarch confirmed


dx set length 48
dx set datareqsubchan 178
dx set baseinitaccum 5
dx set basewarn off
dx set baseerror off

# start it all up

#start tasks
