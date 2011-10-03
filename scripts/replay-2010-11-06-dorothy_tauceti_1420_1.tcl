# Replay setiquest data
# replay-2010-11-06-dorothy_tauceti_1420_1.tcl

act set type target
act set watchdogs off
# set the Explorer Target Id
# tau ceti = 18
act set targetbeam1 130018
act set targetprimary 130018

sched set dxtune range
sched set beam1 on
sched set beam2 off
sched set beam3 off

# beginfreq = CenterFreq - 3.54986666 MHz
# endfreq = CenterFreq + 3.54986666 MHz
sched set beginfreq 1416.45013334
sched set endfreq 1423.5498666
sched set target user
sched set followup off
sched set catshigh nearest,habcat

dx set baseinitaccum 20
dx set baserep 20
dx set daddthresh 8.0
dx set length 139
tscope sim

db set usedb on
db set host localhost
db set name setiquest
