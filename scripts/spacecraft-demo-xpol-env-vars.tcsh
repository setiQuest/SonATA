#!/bin/tcsh

# spacecraft-demo-xpol-env-vars.tcsh

setenv FILTER_DIR "${HOME}/sonata_install/filters"
setenv SSE_SETUP "${HOME}/sonata_install/setup"
#---------------------------------
# Channelizer Environmental Variables
#------------------------------------

# Logical Channelizer Host Names
setenv RUNSSE_CHAN_HOSTS "chanhost1x"
#setenv RUNSSE_CHAN_HOSTS "chanhost1x chanhost1y chanhost2x chanhost2y chanhost3x chanhost3y"

# Actual Channelizer Host Names
setenv CHANHOST1X `hostname`
setenv CHANHOST1X_NAME "chan1x"

# Command line options for all Channelizers
setenv CHANOPTS "-C 8 -c 8 -N 10 -O 0.25 -B 3.2768 -d $FILTER_DIR/LS256c10f25o70d.flt  -w 1 -F 1420.0 -t -1"

# Beam Specific Command Line options for Channelizers
setenv CHANHOST1X_OPTS "-P X  -i 50000 -j 51000 -I 226.1.50.1 -J 227.1.1.1"
setenv CHANHOST1Y_OPTS "-P Y  -i 50001 -j 51000 -I 226.1.50.2 -J 227.1.1.1"
setenv CHANHOST2X_OPTS "-P X  -i 50002 -j 52000 -I 226.2.50.1 -J 227.2.1.1"
setenv CHANHOST2Y_OPTS "-P Y  -i 50003 -j 52000 -I 226.2.50.2 -J 227.2.1.1"
setenv CHANHOST3X_OPTS "-P X  -i 50004 -j 53000 -I 226.3.50.1 -J 227.3.1.1"
setenv CHANHOST3Y_OPTS "-P Y  -i 50005 -j 53000 -I 226.3.50.2 -J 227.3.1.1"

#---------------------------
# DX Environmental Variables
#---------------------------

# Logical Host Names
setenv RUNSSE_DX_HOSTS	"dxhost1"

# Actual Host Names
setenv DXHOST1 `hostname`

# DX names
setenv DXHOST1_DX_NAMES	"dx1000" 
# dx1002 dx1003 dx1004 dx1005"
setenv DXHOST2_DX_NAMES	"dx2001 dx2002 dx2003 dx2004 dx2005 dx2006"
#setenv DXHOST2_DX_NAMES	"dx1001 dx1002 dx1003 dx1004 dx1005 dx1006"

# Command line arguments for all DXs
setenv DX_OPTS "-f 10 -z $FILTER_DIR/LS256c10f25o70d.flt -F 64 -w .4096 -T 1024"

# Beam Specific Command line options for DXs
setenv BEAM1_DX_OPTS "-j 51100 -J 229.1.1.1"
setenv BEAM2_DX_OPTS "-j 52000 -J 229.2.1.1"
setenv BEAM3_DX_OPTS "-j 53000 -J 229.3.1.1"

# Single Polarization Only Options
setenv XPOL_ONLY "-p x"
setenv YPOL_ONLY "-p y"

# Set up command line arguments for each DX_HOST
# BEAM1 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM1_DX_OPTS"
# BEAM2 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM2_DX_OPTS"
# BEAM3 setenv DX_HOSTn_OPTS "$DX_OPTS $BEAM3_DX_OPTS"
# XPOL_ONLY or YPOL_ONLY if appropriate

setenv DXHOST1_OPTS "$DX_OPTS $BEAM1_DX_OPTS $XPOL_ONLY"

# Backend Server Host for use with ATA
#-------------------------------------
#setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST sonata

# Backend Server Host for offline testing
setenv CONTROL_COMPONENTS_ANT_CONTROL_HOST localhost
${HOME}/sonata_install/scripts/switchConfigFile-1beam-1dx.tcsh
