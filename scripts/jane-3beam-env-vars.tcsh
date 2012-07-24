#!/bin/tcsh

# jane-3beam-env-vars.tcsh

setenv FILTER_DIR "${HOME}/sonata_install/filters"
setenv SSE_SETUP "${HOME}/sonata_install/setup"
#---------------------------------
# Channelizer Environmental Variables
#------------------------------------

# Logical Channelizer Host Names
setenv RUNSSE_CHAN_HOSTS "chanhost1y chanhost2x chanhost3x"
#setenv RUNSSE_CHAN_HOSTS "chanhost1x chanhost1y chanhost2x chanhost2y chanhost3x chanhost3y"

# Actual Channelizer Host Names
setenv CHANHOST1Y "handel"
setenv CHANHOST1Y_NAME "chan1y"
setenv CHANHOST2X "handel"
setenv CHANHOST2X_NAME "chan2x"
setenv CHANHOST3X "brahms"
setenv CHANHOST3X_NAME "chan3x"

# Command line options for all Channelizers
setenv CHANOPTS "-C 8 -c 4 -N 10 -O 0.25 -B 3.2768 -d $FILTER_DIR/LS256c10f25o70d.flt  -w 1 -F 1420.0 -n"

# Beam Specific Command Line options for Channelizers
setenv CHANHOST1X_OPTS "-P X  -i 50100 -j 51100 -I 228.1.50.1 -J 229.1.1.1"
setenv CHANHOST1Y_OPTS "-P Y  -i 50101 -j 51100 -I 228.1.50.2 -J 229.1.1.1"
setenv CHANHOST2X_OPTS "-P X  -i 50100 -j 52100 -I 228.1.50.1 -J 229.2.1.1"
setenv CHANHOST2Y_OPTS "-P Y  -i 50103 -j 52100 -I 228.1.50.2 -J 229.2.1.1"
setenv CHANHOST3X_OPTS "-P X  -i 50100 -j 53100 -I 228.1.50.1 -J 229.3.1.1"
setenv CHANHOST3Y_OPTS "-P Y  -i 50105 -j 53100 -I 228.1.50.2 -J 229.3.1.1"

#---------------------------
# DX Environmental Variables
#---------------------------

# Logical Host Names
setenv RUNSSE_DX_HOSTS	"dxhost1 dxhost2 dxhost3"

# Actual Host Names
setenv DXHOST1 "liszt"
setenv DXHOST2 "liszt"
setenv DXHOST3 "liszt"

# DX names
setenv DXHOST1_DX_NAMES	"dx1000 dx1001" 
setenv DXHOST2_DX_NAMES	"dx2000 dx2001"
setenv DXHOST3_DX_NAMES	"dx3000 dx3001"

# Command line arguments for all DXs
setenv DX_OPTS "-f 10 -z $FILTER_DIR/LS256c10f25o70d.flt"

# Beam Specific Command line options for DXs
setenv BEAM1_DX_OPTS "-j 51100 -J 229.1.1.1"
setenv BEAM2_DX_OPTS "-j 52100 -J 229.2.1.1"
setenv BEAM3_DX_OPTS "-j 53100 -J 229.3.1.1"

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
setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST localhost
${HOME}/sonata_install/scripts/switchConfigFile-jane-test.tcsh
