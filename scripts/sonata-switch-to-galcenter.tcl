# sonata-switch-to-galcenter.tcl
#
# make sure current obs have stopped
stop

# wait a few seconds
sh sleep 10

# disarm/rearm alarm and send email
exec setAlarm ARM,sonata,Switching To GALCENTER

# change database to galcenter
db set host sse100
db set name galcenter2

# change catalog priorities
sched set catshigh galcentergrid2
sched set catslow exoplanets,habcat,tycho2subset,tycho2remainder
#sched set targetmerit primaryid,catalog,meridian,completelyobs,timeleft
sched set targetmerit {primaryid,catalog,completelyobs,timeleft,meridian} current
sched set primaryidcutoff 10 current
# the numbers will come from Gerry
sched set comcalinterval 60.000000000 current
sched set comcallength 2.000000000 current
#sched set mintargetsep 3 current
sched set mintargetsep 2 current
sched set comcal on

#set minNumberReservedFollowups to 4
#sched set minNumberReservedFollowups 4 current

#set minNumberReservedFollowups to 2
sched set minNumberReservedFollowups 2 current
verbose level 2
# restart observing
start obs
