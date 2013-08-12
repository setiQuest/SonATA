# 1 beam
# mars-2beam-beam1-beam3-8405-8445.tcl

# send out "taking the array" email
#sh echo " SonATA taking array" | mailx -s 'SonATA taking array for Testing' -r jjordan@seti.org ata-staff@seti.org

# connect to telescope array

# allow some setup time
act set delay 15

sched set beam1 on
sched set beam2 off
sched set beam3 on

# 24 dxs 800 KHz
sched set beginfreq 8405.0 
sched set endfreq 8445.2
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe on
sched set followup off
sched set multitarget off

db set host sse100
db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  1a
tscope set antsxpol 1a
tscope set antsypol 1a
# both tunings must be the same
 tscope assign beamxc1 1a
 tscope assign beamyc1 1a
 tscope assign beamxd1 1a
 tscope assign beamyd1 1a
 tscope assign beamxd2 1a
 tscope assign beamyd2 1a

act set targetbeam1 170
act set targetbeam2 170
act set targetbeam3 170
act set targetprimary 170
act set type target
act set candarch confirmed
act set multitargetnulls off
act set offactnulls none


dx set length 93
dx set datareqsubchan 178
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off
dx set daddres 4
dx set maxdrifttol 10


