# SSE seeker commands to end observing

# sonata-restart-obs.tcl

stop


sh sleep 5

# release ant array resources
freeants

sh sleep 2


exec setAlarm ARM,sonata,SonATA restarting.

# disconnect from telescope array
tscope cleanup

sh sleep 120

