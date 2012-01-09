
# 3 beam

# 3beam-dualpol-amc-7.tcl



sched set beam1 on
sched set beam2 on
sched set beam3 on

# 24 dxs 800 KHz
sched set beginfreq 3699.0 
sched set endfreq 3719.1
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe on
sched set followup on
sched set catshigh spacecraft,habcat
sched set catslow tycho2subset,tycho2remainder
sched set multitarget on

db set host sse100
db set name sonatadb
db set usedb on

act set type rftestzerodrift
act set candarch confirmed
act set delay 20
act set multitargetnulls off



dx set length 94
dx set datareqsubchan 178
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off

tscope point 1a azel 201.0271 41.1912
