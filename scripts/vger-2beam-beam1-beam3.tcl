# 2 beams
# data-capture-kepler-2beam.tcl
#antenna selection done elsewhere

act set targetbeam1 131
act set targetbeam3 131
act set targetprimary 131
act set type target
act set candarch all
act set multitargetnulls off
act set offactnulls none
act set delay 20

sched set beginfreq 8412.0 
sched set endfreq 8432.0
sched set dxtune range
sched set rftune auto
sched set target user
sched set beam1 on
sched set beam2 off
sched set beam3 on
sched set followup on
sched set multitarget on


dx set length 94
dx set datareqsubchan 384
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off
dx set datareqsubband 1535 max
dx set pulsethresh 12.0
dx set tripletthresh 30
dx set maxcand 8

db set name sonatadb
db set usedb on
db set user sonata
db set host sse100

