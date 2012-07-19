# SSE seeker commands to end observing

# sonata-stop-rfiscan.tcl

stop

sh sleep 5

# release ant array resources
freeants

sh sleep 2

# send out "finished with array" email
#sh echo "SonATA done with the array " | mailx -s 'SonATA finished with array' -r jjordan@seti.org ata-staff@seti.org

exec setAlarm ARM,sonata,SonATA done. Array Is Free

# disconnect from telescope array
tscope cleanup


