# sonata-switch-to-kepler.tcl
#
# make sure current obs have stopped
stop

# wait a few seconds
sh sleep 10

# disarm/rearm alarm and send email
#sh echo "SonATA switching to Kepler" | mailx -s 'SonATA switching to Kepler' -r jjordan@seti.org ata-staff@seti.org
exec setAlarm ARM,sonata,Switching To KEPLER

# change database to kepler
#db set name kepler6667mhz
#db set host sse100
db set host sse100
db set name kepler6667mhz

# change catalog priorities
sched set catshigh keplerHZ,exokepler
sched set catslow exoplanets,habcat,tycho2subset,tycho2remainder
sched set targetmerit catalog,meridian,completelyobs,timeleft
sched set comcal off

# restart observing
start obs
