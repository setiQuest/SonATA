# vger-demo-xpol.tcl
# runpacketsend-dx-vger-xpol -> dx1000

act set type iftest
act set watchdogs off
act set delay 20

sched set dxtune user
sched set beam1 on
sched set beam2 off
sched set beam3 off

dx set basewarn off
dx set baseerror off

dx set baseinitaccum 5
dx set baserep 20
dx load chan 0 dx1000
dx load skyfreq 8419.530000000 dx1000
dx load chan 0 dx1000
dx set datareqsubchan 406
dx set length 94

exec runpacketsend-dx-vger-xpol.tcsh &
start obs
