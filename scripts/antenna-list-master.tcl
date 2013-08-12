#JR - April 8, 2013 - This is a raw antenna list made up from the tsys measurements.
#JR June 6, 2013 - Removed 2m so Gerry can work on it.
#JR June 6, 2013 - Removed 1a - it is flakey and we think heat may be affecting it. Removing now since
#  we are expecting a heat wave. Need to add it back when it is deemed OK.
#JR June 8, 2013 - Removed 3j from both pols. It is a hot day 100 degrees. bfplot showed 3j may be the culprit.
# JR - Also the 5e-3l baseline shows as bad in bfplot - removing 3l - it showed an elevation of -23.0503
#JR - June 12, 2013 - BF1 problems. Disabled BF1
#JR - june 14, 2013 - BF1 should work now.
#JR - June 15, 2013 - 2gy bad 
#JR - June 19, 2013 - Added 2m

tscope set antsprimary 1c,1d,1e,1h,1k,2e,2f,2g,2j,2k,2m,3d,4l,5b,5c,5e
tscope set antsxpol 1b,1c,1d,1h,1k,2e,2f,2g,2j,2k,2m,3d,4l,5b,5c,5e
tscope set antsypol 1c,1e,1h,1k,2e,2f,2j,2k,3d,4l,5b,5c,5e

# ANTS MARKED GOOD (BF1)
#Removed 2m from the list based on Chris. Munson's message(6/23). Anu 6/27
#JR - July 2, 2013 - swapped channelizer sleds so we can run BEAM2 instead of BEAM1 (BF1 is broken)
#tscope assign beamxc1 1b,1c,1d,1h,1k,2e,2f,2g,2j,2k,2m,3d,4l,5b,5c,5e
#JJ - July 3 back to beams 1 & 3
#tscope assign beamxc1 1b,1c,1d,1h,1k,2e,2f,2g,2j,2k,3d,4l,5b,5c,5e
#tscope assign beamyc1 1c,1d,1e,1h,1k,2e,2f,2j,2k,3d,4l,5b,5c,5e
#JR - July 19, 2013 - Beam 3 now used.
tscope assign beamxc1 1c,1d,1h,1k,2e,2f,2g,2j,2k,2m,3d,4l,5b,5c,5e
tscope assign beamyc1 1c,1d,1e,1h,1k,2e,2f,2j,2k,3d,4l,5b,5c,5e

# ANTS MARKED GOOD (BF2) 
#Removed 2m from the list based on Chris. Munson's message(6/23). Anu 6/27
#JR - July 2, 2013 - swapped channelizer sleds so we can run BEAM2 instead of BEAM1 (BF1 is broken)
#JJ - July 3 back to beams 1 & 3
tscope assign beamxd1 1c,1d,1h,1k,2e,2f,2g,2j,2m,2k,3d,4l,5b,5c,5e
tscope assign beamyd1 1c,1e,1h,1k,2e,2f,2j,2k,3d,4l,5b,5c,5e

# ANTS MARKED GOOD (BF3)
#Removed 2m from the list based on Chris. Munson's message(6/23). Anu 6/27
#tscope assign beamxd2 1b,1c,1d,1h,1k,2e,2f,2g,2j,2k,2m,3d,4l,5b,5c,5e
#JR - July 19, 2013 - Beam 3 bad - b13.bfa malfunction
#tscope assign beamxd2 1c,1d,1h,1k,2e,2f,2g,2j,2k,2m,3d,4l,5b,5c,5e
#tscope assign beamyd2 1c,1e,1h,1k,2e,2f,2j,2k,3d,4l,5b,5c,5e

