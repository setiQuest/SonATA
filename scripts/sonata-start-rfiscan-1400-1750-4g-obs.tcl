
# 2 beam
# sonata-start-rfiscan-1400-1750-4g-obs.tcl

# send out "taking the array" email
sh echo "SonATA taking 4g" | mailx -s 'SonATA taking 4g for Rfi Scan' -r jjordan@seti.org ata-staff@seti.org

# connect to telescope array
#tscope setup

# allow some setup time
sh sleep 2

sched set beam1 on
sched set beam2 on
sched set beam3 off
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay

sched set beginfreq 1400.0 
sched set endfreq 1750.0
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe on
sched set followup off
sched set catshigh spacecraft,habcat
sched set catslow tycho2subset,tycho2remainder
sched set multitarget off

db set host sse100
db set name keplerLBand
db set usedb on


tscope set antlistsource param
tscope set antsprimary 4g
tscope set antsxpol 4g
tscope set antsypol 4g
 tscope assign beamxc1 4g
 tscope assign beamyc1 4g
 tscope assign beamxd1 4g
 tscope assign beamyd1 4g
 #tscope assign beamxd2 4g
 #tscope assign beamyd2 4g


act set type rfiscan


dx set length 94
dx set datareqsubchan 178
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off

#tscope point 4g azel 180.0 84.0
