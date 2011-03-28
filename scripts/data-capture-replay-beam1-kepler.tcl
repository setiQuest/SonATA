# data-capture-replay-beam1-kepler.tcl

act set type target
act set watchdogs off
act set targetbeam1 173
act set targetprimary 173

sched set dxtune range
sched set beginfreq 8422.653892
sched set endfreq 8429.7043333
sched set target user
sched set beam1 on
sched set beam2 off
sched set beam3 off

dx set basewarn off
dx set baseerror off

dx set baseinitaccum 5
dx set baserep 5
dx set datareqsubchan 406
dx set length 13

db set name sonatadb
db set usedb on
db set user sonata
db set host sse100
