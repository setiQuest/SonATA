# sonata-switch-to-exoplanets.tcl
#
# make sure current obs have stopped
stop

# wait a few seconds
sh sleep 10

# disarm/rearm alarm and send email
#sh echo "SonATA switching to Exoplanets" | mailx -s 'SonATA switching to Exoplanets' -r jjordan@seti.org ata-staff@seti.org
exec setAlarm ARM,sonata,Switching To exoplanets

# change database to exoplanets
db set name exoplanets201205

# change catalog priorities
sched set catshigh exoplanets,habcat
sched set targetmerit catalog,completelyobs,timeleft,meridian

# restart observing
start obs
