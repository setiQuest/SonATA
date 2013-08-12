#!/bin/tcsh

# 1beam-beam3-dualpol-2dx-1zx-512F-env-vars.tcsh

setenv FILTER_DIR "${HOME}/sonata_install/filters"
setenv SSE_SETUP "${HOME}/sonata_install/setup"

#---------------------------------
# Channelizer Environmental Variables
#------------------------------------

# Logical Channelizer Host Names
setenv RUNSSE_CHAN_HOSTS "chanhost3x chanhost3y"
#setenv RUNSSE_CHAN_HOSTS "chanhost1x chanhost1y chanhost2x chanhost2y chanhost3x chanhost3y"

# Actual Channelizer Host Names
setenv CHANHOST3X "seti002-1"
setenv CHANHOST3Y "seti002-1"
# Channelizer Names
setenv CHANHOST3X_NAME "chan3x"
setenv CHANHOST3Y_NAME "chan3y"

# Command line options for all Channelizers
setenv CHANOPTS "-C 128 -c 49 -N 10 -O 0.25 -B 104.8576 -d $FILTER_DIR/LS256c10f25o70d.flt  -w 5 -F 1420.0 -t -1 -p"

# Beam Specific Command Line options for Channelizers
setenv CHANHOST1X_OPTS "-P X  -i 50000 -j 51000 -I 226.1.50.1 -J 227.1.1.1"
setenv CHANHOST1Y_OPTS "-P Y  -i 50001 -j 51000 -I 226.1.50.2 -J 227.1.1.1"
setenv CHANHOST2X_OPTS "-P X  -i 50002 -j 52000 -I 226.2.50.1 -J 227.2.1.1"
setenv CHANHOST2Y_OPTS "-P Y  -i 50003 -j 52000 -I 226.2.50.2 -J 227.2.1.1"
setenv CHANHOST3X_OPTS "-P X  -i 50004 -j 53000 -I 226.3.50.1 -J 227.3.1.1"
setenv CHANHOST3Y_OPTS "-P Y  -i 50005 -j 53000 -I 226.3.50.2 -J 227.3.1.1"

# DX Environmental Variables
#---------------------------

# Logical Host Names
setenv RUNSSE_DX_HOSTS	"dxhost7"

# Actual Host Names
setenv DXHOST1 "seti000-1"
setenv DXHOST2 "seti000-3"
setenv DXHOST3 "seti000-4"
setenv DXHOST4 "seti001-2"
setenv DXHOST5 "seti001-3"
setenv DXHOST6 "seti001-4"
setenv DXHOST7 "seti002-2"
setenv DXHOST8 "seti002-3"
setenv DXHOST9 "seti002-4"

# DX names
setenv DXHOST1_DX_NAMES "dx1000 dx1001 dx1002 dx1003 dx1004 dx1005 dx1006 dx1007"
setenv DXHOST2_DX_NAMES "dx1008 dx1009 dx1010 dx1011 dx1012 dx1013 dx1014 dx1015"
setenv DXHOST3_DX_NAMES "dx1016 dx1017 dx1018 dx1019 dx1020 dx1021 dx1022 dx1023"
setenv DXHOST4_DX_NAMES "dx2000 dx2001 dx2002 dx2003 dx2004 dx2005 dx2006 dx2007"
setenv DXHOST5_DX_NAMES "dx2008 dx2009 dx2010 dx2011 dx2012 dx2013 dx2014 dx2015"
setenv DXHOST6_DX_NAMES "dx2016 dx2017 dx2018 dx2019 dx2020 dx2021 dx2022 dx2023"
setenv DXHOST7_DX_NAMES "dx3000 dx3001"
setenv DXHOST8_DX_NAMES "dx3008 dx3009 dx3010 dx3011 dx3012 dx3013 dx3014 dx3015"
setenv DXHOST9_DX_NAMES "dx3016 dx3017 dx3018 dx3019 dx3020 dx3021 dx3022 dx3023"


# Command line arguments for all DXs
setenv DX_OPTS "-f 10 -z $FILTER_DIR/LS256c10f25o70d.flt -F 512 -w .8192 -T 2048"

# Beam Specific Command line options for DXs
setenv BEAM1_DX_OPTS "-j 51000 -J 227.1.1.1"
setenv BEAM2_DX_OPTS "-j 52000 -J 227.2.1.1"
setenv BEAM3_DX_OPTS "-j 53000 -J 227.3.1.1"

# Single Polarization Only Options
setenv XPOL_ONLY "-p x"
setenv YPOL_ONLY "-p y"

# Set up command line arguments for each DX_HOST
# BEAM1 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM1_DX_OPTS"
# BEAM2 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM2_DX_OPTS"
# BEAM3 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM3_DX_OPTS"
# XPOL_ONLY or YPOL_ONLY if appropriate

setenv DXHOST1_OPTS "$DX_OPTS $BEAM1_DX_OPTS" 
setenv DXHOST2_OPTS "$DX_OPTS $BEAM1_DX_OPTS"
setenv DXHOST3_OPTS "$DX_OPTS $BEAM1_DX_OPTS"
setenv DXHOST4_OPTS "$DX_OPTS $BEAM2_DX_OPTS"
setenv DXHOST5_OPTS "$DX_OPTS $BEAM2_DX_OPTS"
setenv DXHOST5_OPTS "$DX_OPTS $BEAM2_DX_OPTS"
setenv DXHOST7_OPTS "$DX_OPTS $BEAM3_DX_OPTS"
setenv DXHOST8_OPTS "$DX_OPTS $BEAM3_DX_OPTS"
setenv DXHOST9_OPTS "$DX_OPTS $BEAM3_DX_OPTS"

# ZX Environmental Variables
# #---------------------------
#
# # Logical Host Names
setenv RUNSSE_ZX_HOSTS  "zxhost3"
#
# # Actual Host Names
# setenv ZXHOST1 "seti000-4"
# setenv ZXHOST2 "seti001-4"
setenv ZXHOST3 "seti002-4"
#
# # ZX names
 setenv ZXHOST1_ZX_NAMES "zx1900"
 setenv ZXHOST2_ZX_NAMES "zx2900"
 setenv ZXHOST3_ZX_NAMES "zx3900"
#
# # Command line arguments for all ZXs
 setenv ZX_OPTS "-Z -h 8890"
#
#
# # Set up command line arguments for each ZX_HOST
# # BEAM1 setenv ZX_HOSTn_OPTS "$DX_OPTS $BEAM1_DX_OPTS"
# # BEAM2 setenv ZX_HOSTn_OPTS "$DX_OPTS $BEAM2_DX_OPTS"
# # BEAM3 setenv ZX_HOSTn_OPTS "$DX_OPTS $BEAM3_DX_OPTS"
# # XPOL_ONLY or YPOL_ONLY if appropriate
#
 setenv ZXHOST1_OPTS "$DX_OPTS $BEAM1_DX_OPTS $ZX_OPTS"
 setenv ZXHOST2_OPTS "$DX_OPTS $BEAM2_DX_OPTS $ZX_OPTS"
 setenv ZXHOST3_OPTS "$DX_OPTS $BEAM3_DX_OPTS $ZX_OPTS"
#
# # Backend Server Host for use with ATA
#
# Backend Server Host for use with ATA
#-------------------------------------
setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST sonata

# switch the expected components config file to match
${HOME}/sonata_install/scripts/switchConfigFile-1beam-beam3-800KHz-2dx-1zx.tcsh
