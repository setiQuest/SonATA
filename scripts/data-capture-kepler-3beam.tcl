# 3 beams
# data-capture-kepler-3beam.tcl
#antenna selection done elsewhere

act set targetbeam1 173
act set targetbeam2 173
act set targetbeam3 173
act set targetprimary 173
act set type target
act set candarch all
act set multitargetnulls off
act set delay 15

sched set beginfreq 8426.0 
sched set endfreq 8426.8
sched set dxtune range
sched set rftune auto
sched set target user
sched set beam1 on
sched set beam2 on
sched set beam3 on
sched set followup on


dx set length 256
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

