# sonata-switch-to-casa-grid.tcl
#
# make sure current obs have stopped
stop

# wait a few seconds
sh sleep 10

# disarm/rearm alarm and send email
#sh echo "SonATA switching to Kepler" | mailx -s 'SonATA switching to Kepler' -r jjordan@seti.org ata-staff@seti.org
exec setAlarm ARM,sonata,Switching To CasA

# change database to kepler
db set name casa

# change catalog priorities
sched set catshigh casagrid,habcat
sched set catslow exoplanets,tycho2subset,tycho2remainder
sched set targetmerit targetid,catalog,completelyobs,timeleft,meridian
sched set comcal on

# restart observing
start obs
