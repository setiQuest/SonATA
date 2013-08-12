# kes-tscope-vger-full-array-2.tcl
# SSE seeker commands to start observing

# send out "taking the array" email
#sh echo "SonATA taking array" | mailx -s 'SonATA Testing taking array for vger' -r kes@smolek.com ata-staff@seti.org

# connect to the backend server
tscope setup

# allow some setup time
sh sleep 2

sched set tasks {autoselectants,prepants,bfreset,bfautoatten,bfinit,caldelay,calphase,calfreq,obs} current 
sched set tscopemaxfailures 10 current 
sched set tscopereadypause 25 current 
sched set zenithavoid 3.000000000 current 
tscope set antlistsource {antgroup} current 
tscope set antsmaxsefd 20000 current 
tscope set antsprimary {antgroup} current 
tscope set antsxpol {antgroup} current 
tscope set antsypol {antgroup} current 
tscope set beamsize 348.000000000 current 
tscope set calcycles 2 current 
tscope set caltime 90 current 
tscope set caltype {delay} current 
tscope set centertuneoffset 0.000000000 current 
tscope set primaryfov 3.500000000 current 
tscope set sitehoriz 18.000000000 current 
tscope set sitelat 40.817361111 current 
tscope set sitelong 121.471802778 current 
tscope set sitename {ATA} current 
tscope set tuneoffset 0.000000000 current 
tscope set tuninga 1420.000000000 current 
tscope set tuningb 1420.000000000 current 
tscope set tuningc 3040.000000000 current 
tscope set tuningd 3040.000000000 current 

# begin observing

start tasks
