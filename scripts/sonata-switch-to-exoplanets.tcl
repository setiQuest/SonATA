# sonata-switch-to-exoplanets.tcl
#
# make sure current obs have stopped
stop

verbose level 2
# wait a few seconds
sh sleep 10

# disarm/rearm alarm and send email
#sh echo "SonATA switching to Exoplanets" | mailx -s 'SonATA switching to Exoplanets' -r jjordan@seti.org ata-staff@seti.org
exec setAlarm ARM,sonata,Switching To exoplanets

# change database to exoplanets
#db set host sse100
#db set name exoplanets6667mhz
db set host sse100
db set name exoplanets6667mhz

# change catalog priorities
sched set catshigh exoplanets,habcat
sched set catslow {tycho2subset,tycho2remainder} current

sched set targetmerit catalog,completelyobs,timeleft,meridian
sched set comcal off

# restart observing
start obs
