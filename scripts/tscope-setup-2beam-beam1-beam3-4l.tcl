# 1 beam -- beam3

# tscope-setup-1beam-beam3-4l.tcl

sh echo "SonATA taking array" | mailx -s 'SonATA taking ant4l, bf1, & bf3 for Testing' -r jjordan@seti.org ata-staff@seti.org

sched set beginfreq 1410.0
sched set endfreq 1450.2
sched set beam1 on
sched set beam2 off
sched set beam3 on
sched set tasks prepants,bfreset,bfautoatten,bfinit,caldelay


db set name sonatadb
db set usedb on

tscope set antlistsource param
tscope set antsprimary  4l
tscope set antsxpol 4l
tscope set antsypol 4l
# both tunings must be the same
tscope set tuningc 1420.0
tscope set tuningd 1420.0
 tscope assign beamxc1 4l
 tscope assign beamyc1 4l
 #tscope assign beamxd1 4l
 #tscope assign beamyd1 4l
 tscope assign beamxd2 4l
 tscope assign beamyd2 4l

