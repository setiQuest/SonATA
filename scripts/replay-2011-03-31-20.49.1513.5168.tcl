# Replay dualbeam data
# replay-2011-03-31-20.49.1513.5168.tcl 

act set type target
act set watchdogs off
act set targetbeam1 1991094
act set targetbeam2 1991098
act set targetprimary 1991094
act set delay 20

channel set delay 6

sched set dxtune range
sched set beam1 on
sched set beam2 on
sched set beam3 off

sched set beginfreq 1509.966934
sched set endfreq 1517.066666

sched set target user
sched set followup on
sched set catshigh nearest,habcat,tycho2subset,tycho2remainder
sched set multitarget on

dx set baseinitaccum 20
dx set baserep 20
dx set daddthresh 8.5
dx set length 141
tscope sim

db set usedb on
db set host sse200
db set name sonatadb
