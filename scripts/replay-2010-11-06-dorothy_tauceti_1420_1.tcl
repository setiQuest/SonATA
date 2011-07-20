# Replay setiquest data
# replay-2010-11-06-dorothy_tauceti_1420_1.tcl

act set type target
act set watchdogs off
act set targetbeam1 130018
act set targetprimary 130018

sched set dxtune range
sched set beam1 on
sched set beam2 off
sched set beam3 off

sched set beginfreq 1416.45016
sched set endfreq 1423.546
sched set target user
sched set followup off
sched set catshigh nearest,habcat

dx set baseinitaccum 20
dx set baserep 20
dx set daddthresh 8.0
dx load skyfreq 1416.723 dx1000 
dx load skyfreq 1417.269 dx1001 
dx load skyfreq 1417.816 dx1002 
dx load skyfreq 1418.362 dx1003 
dx load skyfreq 1418.908 dx1004 
dx load skyfreq 1419.454 dx1005 
dx load skyfreq 1420.546 dx1006 
dx load skyfreq 1421.092 dx1007 
dx load skyfreq 1421.638 dx1008 
dx load skyfreq 1422.185 dx1009 
dx load skyfreq 1422.731 dx1010 
dx load skyfreq 1423.277 dx1011 
dx load chan 0 dx1000 
dx load chan 1 dx1001 
dx load chan 2 dx1002 
dx load chan 3 dx1003 
dx load chan 4 dx1004 
dx load chan 5 dx1005 
dx load chan 6 dx1006 
dx load chan 7 dx1007 
dx load chan 8 dx1008 
dx load chan 9 dx1009 
dx load chan 10 dx1010 
dx load chan 11 dx1011 
dx set length 150
tscope sim

db set usedb on
db set host sse200
db set name setiquest
