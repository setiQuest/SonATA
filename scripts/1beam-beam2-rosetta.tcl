
# 1 beam

# 1beam-beam2-rosetts.tcl


sched set beam1 off
sched set beam2 on
sched set beam3 off

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
sched set multitarget off

db set host sse100
db set name sonatadb
db set usedb on

act set targetbeam1 160
act set targetbeam2 160
act set targetbeam3 160
act set targetprimary 160
act set type target
act set candarch confirmed
act set delay 20
act set multitargetnulls off



dx set length 94
dx set datareqsubchan 178
dx set baseinitaccum 20
dx set basewarn on
dx set baseerror on


