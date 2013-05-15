# 1 beam -- beam 1

# tscope-setup-1beam-1a.tcl

#sh echo "SonATA taking array" | mailx -s 'SonATA taking array for Testing' -r jjordan@seti.org ata-staff@seti.org

sched set beginfreq 1410.0
sched set endfreq 1450.2
sched set beam1 on
sched set beam2 off
sched set beam3 off
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay


db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  1a
tscope set antsxpol 1a
tscope set antsypol 1a
# both tunings must be the same
tscope set tuningc 1420.0
tscope set tuningd 1420.0
 tscope assign beamxc1 1a
 tscope assign beamyc1 1a
 tscope assign beamxd1 1a
 tscope assign beamyd1 1a
 tscope assign beamxd2 1a
 tscope assign beamyd2 1a

