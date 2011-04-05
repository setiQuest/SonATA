# data-capture-replay-beam1-kepler.tcl

act set type target
act set watchdogs off
act set targetbeam1 100031
act set targetprimary 100031

sched set dxtune range
sched set beginfreq 1500.0
sched set endfreq 1520.0
sched set target user
sched set beam1 on
sched set beam2 off
sched set beam3 off

dx set basewarn off
dx set baseerror off

dx set baseinitaccum 10
dx set baserep 20
dx set datareqsubchan 406
dx set length 48

db set name sonatadb
db set usedb on
db set user sonata
db set host sse100
