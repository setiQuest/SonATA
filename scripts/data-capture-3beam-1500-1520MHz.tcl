# 3 beams
# data-capture-3beam-1500-1520MHz.tcl
#antenna selection done elsewhere

act set type target
act set candarch all
act set multitargetnulls on
act set delay 20

sched set beginfreq 1500.0 
sched set endfreq 1520.0
sched set dxtune range
sched set rftune auto
sched set target auto
sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set followup on


dx set length 388
dx set datareqsubchan 384
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off
dx set datareqsubband 1535 max
dx set pulsethresh 12.0
dx set tripletthresh 30
dx set maxcand 20

db set name sonatadb
db set usedb on
db set user sonata
db set host sse100

