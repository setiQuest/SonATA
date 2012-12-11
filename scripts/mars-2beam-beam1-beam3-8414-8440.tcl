# 1 beam
# mars-2beam-beam1-beam3-8414-8440.tcl

# send out "taking the array" email

# connect to telescope array

# allow some setup time
act set delay 20

sched set beam1 on
sched set beam2 off
sched set beam3 on

# 24 dxs 800 KHz
sched set beginfreq 8414.0 
sched set endfreq 8440.2
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe off
sched set followup on
sched set multitarget off

db set host sse300
db set name spacecraft
db set usedb on


act set targetbeam1 170
act set targetbeam2 170
act set targetbeam3 170
act set targetprimary 170
act set type target
act set candarch confirmed
act set multitargetnulls off
act set offactnulls projection


dx set length 93
dx set datareqsubchan 178
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off
dx set daddres 4
dx set maxdrifttol 10
dx set daddthresh 8.0

