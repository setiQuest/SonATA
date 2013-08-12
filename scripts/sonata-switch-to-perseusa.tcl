# sonata-switch-to-perseusa.tcl
#
# make sure current obs have stopped
stop

# wait a few seconds
sh sleep 10

# disarm/rearm alarm and send email
exec setAlarm ARM,sonata,Switching To PerseusA

db set host sse100
db set name perseusa_1500
# change catalog priorities
sched set catshigh {perseusagrid} current
sched set catslow {exoplanets,tycho2subset,tycho2remainder} current
sched set beginfreq 1410
sched set endfreq 1430
sched set multitarget on
sched set target auto
sched set pipe on
# restart observing
start obs
