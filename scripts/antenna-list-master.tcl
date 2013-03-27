#JR - removed 3C and 2GY - Jan 16, 2013 - bad delay cals
#JR - Jan 16, 2013 - Removed 1e, was not responsive, failed recover.
#JR - Jan 16, 2013 - BF1 removed - bad iBOB board
#JJ - Beam 2 repaired and tested 2013-01-30 with new list
#JR - Feb 04, 2013 5h,3j - Bad delay cal and BFPLOT. They were in every pol.
#JR - Feb 06, 2013 Removed 1f, not responsive.
#JR - Feb 19, 2013 - Beam 2 is bad again - Trying to use Beam 1 again.
#JR - March 03, 2013 - Beam 1 is bad!
#JR - March 43, 2013 - Beam 1 should be OK again - 24-port switch died, was relaced.
#JR - March 13, 2013. 2ex,2fx,2cx,2dx should not be used with beam 1. i4.bfa is not operational.
#JR - March 25, 2013 - 5e elevation wont move off of 770 degrees! Removing.


tscope set antsprimary 1a,1c,1d,1e,1g,1k,2c,2e,2f,2g,2j,2k,2m,3c,3d,3e,3j,3l,4e,4g,4h,4k,4l,5b,5c
tscope set antsxpol    1a,1c,1d,2e,2f,2g,2j,2k,2m,3c,3d,3e,3j,3l,4e,4g,4h,4k,4l,5b,5c
tscope set antsypol    1a,1c,1e,1g,1k,2c,2g,2j,3c,3e,3j,4e,4g,4k,4l,5c

# ANTS MARKED GOOD (BF1)
tscope assign beamxc1 1a,1c,1d,2c,2e,2f,2g,2j,2k,2m,3c,3d,3e,3j,3l,4e,4g,4h,4k,4l,5b,5c
tscope assign beamyc1 1a,1c,1e,1g,1k,2c,2g,2j,3c,3e,3j,4e,4g,4k,4l,5c

# ANTS MARKED GOOD (BF2)
#tscope assign beamxd1 1a,1c,1d,2c,2e,2f,2g,2j,2k,2m,3c,3d,3e,3j,3l,4e,4g,4h,4k,4l,5b,5c
#tscope assign beamyd1 1a,1c,1e,1g,1k,2c,2g,2j,3c,3e,3j,4e,4g,4k,4l,5c

# ANTS MARKED GOOD (BF3)
tscope assign beamxd2 1a,1c,1d,2e,2f,2g,2j,2k,2m,3c,3d,3e,3j,3l,4e,4g,4h,4k,4l,5b,5c
tscope assign beamyd2 1a,1c,1e,1g,1k,2c,2g,2j,3c,3e,3j,4e,4g,4k,4l,5c

