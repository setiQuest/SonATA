 sched set beam1 on
 sched set beam2 on
 sched set beam3 off
tscope sim
 dx set length 25
 sched set dxtune range
 sched set multitarget on
 db set name sonatadb
 db set usedb on
 db set host localhost
 act set rfiagelimit 365 max
 act set rfiagelimit 80 
 db set usedb on
 db set host localhost
 sched set target user
 act set type target
 sched set beginfreq 8401.4
 sched set endfreq 8421.4
 act set targetbeam1 144
 act set targetbeam2 144
 act set targetprimary 144
