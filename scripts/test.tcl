tscope sim
 sched set beam1 on
 sched set beam2 off
 sched set beam3 on
 dx set length 93
 sched set dxtune range
 sched set multitarget on
 db set name sonatadb
 db set usedb on
 db set host sse100
 db set user sonata
 db set usedb on
 dx set basewarn off
 dx set baseerror off
 sched set target auto
 act set type target
 sched set beginfreq 3800.000
 sched set endfreq 4200.000
 sched set pipe on
 sched set followup on
 sched set catshigh exoplanets,habcat
 sched set catslow tycho2subset,tycho2remainder
 sched set tasks calphase,calfreq
 act set targetbeam1 144
 act set targetbeam2 144
 act set targetprimary 144
 verbose level 2
 dx set datareqmaxcompampsubchan 12 max
 dx set datareqmaxcompampsubchan 12
