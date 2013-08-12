# 1 beam
# sonata-start-rosetta-beam2-good-ants.obs.tcl

# send out "taking the array" email
#sh echo " SonATA taking array" | mailx -s 'SonATA taking array for Rosetta Test' -r jjordan@seti.org ata-staff@seti.org

# connect to telescope array
#tscope setup

# allow some setup time
sh sleep 2
act set delay 15

sched set beam1 off
sched set beam2 on
sched set beam3 off
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq

# 24 dxs 800 KHz
sched set beginfreq 8412.0 
sched set endfreq 8440.1
sched set dxtune range
sched set rftune auto
sched set target user
sched set pipe on
sched set followup off

db set host sse100
db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope set antsxpol 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope set antsypol 1a,1c,1d,2a,2e,5b,5c,5e
# both tunings must be the same
tscope set tuningc 8423.0
tscope set tuningd 8423.0
tscope assign beamxc1 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope assign beamyc1 1a,1c,1d,2a,2e,5b,5c,5e
tscope assign beamxd1 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope assign beamyd1 1a,1c,1d,2a,2e,5b,5c,5e
tscope assign beamxd2 1a,1c,1d,1f,2a,2b,2e,2f,2g,2j,2m,3d,5b,5c,5e
tscope assign beamyd2 1a,1c,1d,2a,2e,5b,5c,5e

set targetbeam2 160
act set targetprimary 160
act set type target
act set candarch confirmed


dx set length 93
dx set datareqsubchan 178
dx set baseinitaccum 20
dx set basewarn off
dx set baseerror off

# start it all up

#start tasks
