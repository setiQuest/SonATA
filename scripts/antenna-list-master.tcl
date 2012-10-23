# makemaster 1ax,1cx,1dx,1fx,1gx,1kx,2bx,2cx,2ex,2gx,2mx,3dx,3ex,3jx,3lx,4ex,4kx,4lx,5bx,5cx,1ay,1cy,1dy,1ey,1fy,1gy,1ky,2by,2cy,2ey,2gy,3dy,3ey,3jy,3ly,4ky,4ly,5by,5cy > antenna-list-master.tcl

# 2012-08-30 5g,2f,2j,2k removed to maint
# 1f,1k restored to list
# 2012-09-25 1d, 1g, 3j in maint, added 4g, 5h
# 2012-09-26 4g removed, but it didn't help cals
# 2012-09-27 5h in maint, 2m removed (not in billy's list)
tscope set antsprimary 1a,1c,1e,1f,1k,2b,2c,2e,2g,3d,3e,3l,4e,4g,4k,4l,5b,5c
tscope set antsxpol 1a,1c,1f,1k,2b,2c,2e,2g,3d,3e,3l,4e,4g,4k,4l,5b,5c
tscope set antsypol 1a,1c,1e,1f,1k,2b,2c,2e,2g,3d,3e,3l,4g,4k,4l,5b,5c

# ANTS MARKED GOOD (BF1)
tscope assign beamxc1 1a,1c,1f,1k,2b,2c,2e,2g,3d,3e,3l,4e,4g,4k,4l,5b,5c
tscope assign beamyc1 1a,1c,1e,1f,1k,2b,2c,2e,2g,3d,3e,3l,4g,4k,4l,5b,5c

# ANTS MARKED GOOD (BF2)
tscope assign beamxd1 1a,1c,1f,1k,2b,2c,2e,2g,3d,3e,3l,4e,4g,4k,4l,5b,5c
tscope assign beamyd1 1a,1c,1e,1f,1k,2b,2c,2e,2g,3d,3e,3l,4g,4k,4l,5b,5c

# ANTS MARKED GOOD (BF3)
#removed 2gy [i17.bfb is failing] 2012-08-31
tscope assign beamxd2 1a,1c,1f,1k,2b,2c,2e,2g,3d,3e,3l,4e,4g,4k,4l,5b,5c
tscope assign beamyd2 1a,1c,1e,1f,1k,2b,2c,2e,2g,3d,3e,3l,4g,4k,4l,5b,5c

