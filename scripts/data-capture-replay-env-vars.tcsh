#!/bin/tcsh

# data-capture-replay-env-vars.tcsh

setenv FILTER_DIR "${HOME}/sonata_install/filters"
setenv SSE_SETUP "${HOME}/sonata_install/setup"
#---------------------------------
# Channelizer Environmental Variables
#------------------------------------

# Logical Channelizer Host Names
setenv RUNSSE_CHAN_HOSTS "chanhost1x"
#setenv RUNSSE_CHAN_HOSTS "chanhost1x chanhost1y chanhost2x chanhost2y chanhost3x chanhost3y"

# Actual Channelizer Host Names
setenv CHANHOST1X "seti000-1"
setenv CHANHOST1X_NAME "chan1x"

# Command line options for all Channelizers
setenv CHANOPTS "-C 16 -c 13 -N 10 -O 0.25 -B 8.73813333 -d $FILTER_DIR/LS256c10f25o70d.flt  -w 1 -F 1420.0 -t -1"

# Beam Specific Command Line options for Channelizers
setenv CHANHOST1X_OPTS "-P X  -i 50100 -j 51100 -I 228.1.50.1 -J 229.1.1.1"

#---------------------------
# DX Environmental Variables
#---------------------------

# Logical Host Names
setenv RUNSSE_DX_HOSTS	"dxhost1 dxhost2"

# Actual Host Names
setenv DXHOST1 "seti000-2"
setenv DXHOST2 "seti000-4"

# DX names
setenv DXHOST1_DX_NAMES	"dx1000 dx1001 dx1002 dx1003 dx1004 dx1005" 
setenv DXHOST2_DX_NAMES	"dx1006 dx1007 dx1008 dx1009 dx1010 dx1011"

# Command line arguments for all DXs
setenv DX_OPTS "-f 10 -z $FILTER_DIR/LS256c10f25o70d.flt -w .546133333 -T 1024 -F 128"

# Beam Specific Command line options for DXs
setenv BEAM1_DX_OPTS "-j 51100 -J 229.1.1.1"

# Single Polarization Only Options
setenv XPOL_ONLY "-p x"
setenv YPOL_ONLY "-p y"

# Set up command line arguments for each DX_HOST
# BEAM1 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM1_DX_OPTS"
# BEAM2 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM2_DX_OPTS"
# BEAM3 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM3_DX_OPTS"
# XPOL_ONLY or YPOL_ONLY if appropriate

setenv DXHOST1_OPTS "$DX_OPTS $BEAM1_DX_OPTS $XPOL_ONLY"
setenv DXHOST2_OPTS "$DX_OPTS $BEAM1_DX_OPTS $XPOL_ONLY"

# Backend Server Host for use with ATA
#-------------------------------------
setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST localhost

# Backend Server Host for offline testing
setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST localhost
${HOME}/sonata_install/scripts/switchConfigFile-setiquest.tcsh
