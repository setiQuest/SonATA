#!/bin/tcsh

# data-collect-2beam-env-vars.tcsh

setenv FILTER_DIR "${HOME}/sonata_install/filters"
setenv SSE_SETUP "${HOME}/sonata_install/setup"
#---------------------------------
# Channelizer Environmental Variables
#------------------------------------

# Logical Channelizer Host Names
setenv RUNSSE_CHAN_HOSTS "chanhost1x chanhost2x"
#setenv RUNSSE_CHAN_HOSTS "chanhost1x chanhost1y chanhost2x chanhost2y chanhost3x chanhost3y"

# Actual Channelizer Host Names
setenv CHANHOST1X "seti000-2"
setenv CHANHOST1X_NAME "chan1x"
setenv CHANHOST2X "seti001-1"
setenv CHANHOST2X_NAME "chan2x"
setenv CHANHOST3X "seti002-1"
setenv CHANHOST3X_NAME "chan3x"

# Command line options for all Channelizers
setenv CHANOPTS "-C 16 -c 2 -N 10 -O 0.25 -B 104.8576 -d $FILTER_DIR/LS256c10f25o70d.flt  -w 5 -F 1420.0 -t -1"

# Beam Specific Command Line options for Channelizers
setenv CHANHOST1X_OPTS "-P X  -i 50000 -j 51100 -I 226.1.50.1 -J 229.1.1.1"
setenv CHANHOST1Y_OPTS "-P Y  -i 50001 -j 51100 -I 226.1.50.2 -J 229.1.1.1"
setenv CHANHOST2X_OPTS "-P X  -i 50002 -j 52100 -I 226.2.50.1 -J 229.2.1.1"
setenv CHANHOST2Y_OPTS "-P Y  -i 50003 -j 52100 -I 226.2.50.2 -J 229.2.1.1"
setenv CHANHOST3X_OPTS "-P X  -i 50004 -j 53100 -I 226.3.50.1 -J 229.3.1.1"
setenv CHANHOST3Y_OPTS "-P Y  -i 50005 -j 53100 -I 226.3.50.2 -J 229.3.1.1"

#---------------------------
# DX Environmental Variables
#---------------------------

# Logical Host Names
setenv RUNSSE_DX_HOSTS	"dxhost1 dxhost2"
#setenv RUNSSE_DX_HOSTS	"dxhost1 dxhost2 dxhost3"

# Actual Host Names
setenv DXHOST1 "seti000-1"
setenv DXHOST2 "seti001-2"
setenv DXHOST3 "seti002-2"

# DX names
setenv DXHOST1_DX_NAMES	"dx1000" 
setenv DXHOST2_DX_NAMES	"dx2000"
setenv DXHOST3_DX_NAMES	"dx3000"

# Command line arguments for all DXs
setenv DX_OPTS "-f 10 -z $FILTER_DIR/LS256c10f25o70d.flt -F 256 -w .8192 -T 2048"

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

setenv DXHOST1_OPTS "$DX_OPTS $BEAM1_DX_OPTS $YPOL_ONLY"
setenv DXHOST2_OPTS "$DX_OPTS $BEAM2_DX_OPTS $XPOL_ONLY"
setenv DXHOST3_OPTS "$DX_OPTS $BEAM3_DX_OPTS $XPOL_ONLY"

# Backend Server Host for use with ATA
#-------------------------------------
#setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST sonata

# Backend Server Host for offline testing
setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST sonata
${HOME}/sonata_install/scripts/switchConfigFile-2beam-data-collect-2dx.tcsh
