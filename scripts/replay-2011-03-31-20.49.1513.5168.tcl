# Replay dualbeam data
# replay-2011-03-31-20.49.1513.5168.tcl 

act set type target
act set watchdogs off
act set targetbeam1 1991094
act set targetbeam2 1991098
act set targetprimary 1991094
<<<<<<< HEAD
act set delay 20
=======
act set delay 25
>>>>>>> 2b62717722555a9686798fb3af6de998bf7735da

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
dx set daddthresh 9.0
dx set length 141
dx set baseerrormeanlower 30
dx set baseerrormeanupper 5000
dx set baseerrorrange 2000
dx set basewarnmeanlower 60
dx set basewarnmeanupper 3000
dx set basewarnrange 1000
tscope sim

db set usedb on
db set host localhost
db set name sonatadb

