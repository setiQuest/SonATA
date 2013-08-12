#!/bin/sh

# SonATA DX environment variables
# 3beam-dualpol-800KHz-72dx-512f-env-vars-batch
#---------------------------------

FILTER_DIR="${HOME}/sonata_install/filters"
SSE_SETUP="${HOME}/sonata_install/setup"
export SSE_SETUP
export FILTER_DIR
# Channelizer Environmental Variables
#------------------------------------

# Logical Channelizer Host Names
RUNSSE_CHAN_HOSTS="chanhost1x chanhost1y chanhost2x chanhost2y chanhost3x chanhost3y"
export RUNSSE_CHAN_HOSTS

# Actual Channelizer Host Names
CHANHOST1X="seti000-2"
CHANHOST2X="seti001-1"
CHANHOST3X="seti002-1"
CHANHOST1Y="seti000-2"
CHANHOST2Y="seti001-1"
CHANHOST3Y="seti002-1"
export CHANHOST1X
export CHANHOST2X
export CHANHOST3X
export CHANHOST1Y
export CHANHOST2Y
export CHANHOST3Y
# Channelizer Names
CHANHOST1X_NAME="chan1x"
CHANHOST2X_NAME="chan2x"
CHANHOST3X_NAME="chan3x"
CHANHOST1Y_NAME="chan1y"
CHANHOST2Y_NAME="chan2y"
CHANHOST3Y_NAME="chan3y"
export CHANHOST1X_NAME
export CHANHOST2X_NAME
export CHANHOST3X_NAME
export CHANHOST1Y_NAME
export CHANHOST2Y_NAME
export CHANHOST3Y_NAME

# Command line options for all Channelizers
CHANOPTS="-C 128 -c 49 -N 10 -O 0.25 -B 104.8576 -d $FILTER_DIR/LS256c10f25o70d.flt  -w 5 -F 1420.0 -t -1 -p"
export CHANOPTS

# Beam Specific Command Line options for Channelizers
CHANHOST1X_OPTS="-P X  -i 50000 -j 51000 -I 226.1.50.1 -J 227.1.1.1"
CHANHOST1Y_OPTS="-P Y  -i 50001 -j 51000 -I 226.1.50.2 -J 227.1.1.1"
CHANHOST2X_OPTS="-P X  -i 50002 -j 52000 -I 226.2.50.1 -J 227.2.1.1"
CHANHOST2Y_OPTS="-P Y  -i 50003 -j 52000 -I 226.2.50.2 -J 227.2.1.1"
CHANHOST3X_OPTS="-P X  -i 50004 -j 53000 -I 226.3.50.1 -J 227.3.1.1"
CHANHOST3Y_OPTS="-P Y  -i 50005 -j 53000 -I 226.3.50.2 -J 227.3.1.1"

export CHANHOST1X_OPTS
export CHANHOST1Y_OPTS
export CHANHOST2X_OPTS
export CHANHOST2Y_OPTS
export CHANHOST3X_OPTS
export CHANHOST3Y_OPTS
# DX Environmental Variables
#---------------------------

# Logical Host Names
RUNSSE_DX_HOSTS="dxhost1 dxhost2 dxhost3 dxhost4 dxhost5 dxhost6 dxhost7 dxhost8 dxhost9"
export RUNSSE_DX_HOSTS

# Actual Host Names
DXHOST1="seti000-1"
DXHOST2="seti000-3"
DXHOST3="seti000-4"
DXHOST4="seti001-2"
DXHOST5="seti001-3"
DXHOST6="seti001-4"
DXHOST7="seti002-2"
DXHOST8="seti002-3"
DXHOST9="seti002-4"
export DXHOST1
export DXHOST2
export DXHOST3
export DXHOST4
export DXHOST5
export DXHOST6
export DXHOST7
export DXHOST8
export DXHOST9
# DX names
DXHOST1_DX_NAMES="dx1000 dx1001 dx1002 dx1003"
DXHOST2_DX_NAMES="dx1008 dx1009 dx1010 dx1011"
DXHOST3_DX_NAMES="dx1016 dx1017 dx1018 dx1019"
DXHOST4_DX_NAMES="dx2000 dx2001 dx2002 dx2003"
DXHOST5_DX_NAMES="dx2008 dx2009 dx2010 dx2011"
DXHOST6_DX_NAMES="dx2016 dx2017 dx2018 dx2019"
DXHOST7_DX_NAMES="dx3000 dx3001 dx3002 dx3003"
DXHOST8_DX_NAMES="dx3008 dx3009 dx3010 dx3011"
DXHOST9_DX_NAMES="dx3016 dx3017 dx3018 dx3019"

export DXHOST1_DX_NAMES
export DXHOST2_DX_NAMES
export DXHOST3_DX_NAMES
export DXHOST4_DX_NAMES
export DXHOST5_DX_NAMES
export DXHOST6_DX_NAMES
export DXHOST7_DX_NAMES
export DXHOST8_DX_NAMES
export DXHOST9_DX_NAMES

# Command line arguments for all DXs
DX_OPTS="-f 10 -z $FILTER_DIR/LS256c10f25o70d.flt -F 512 -w .8192 -T 2048"

# Beam Specific Command line options for DXs
BEAM1_DX_OPTS="-j 51000 -J 227.1.1.1"
BEAM2_DX_OPTS="-j 52000 -J 227.2.1.1"
BEAM3_DX_OPTS="-j 53000 -J 227.3.1.1"

# Single Polarization Only Options
XPOL_ONLY="-p x"
YPOL_ONLY="-p y"

# Set up command line arguments for each DX_HOST
# BEAM1 DX_HOSTn_OPTS="$DX_OPTS $BEAM1_DX_OPTS"
# BEAM2 DX_HOSTn_OPTS="$DX_OPTS $BEAM2_DX_OPTS"
# BEAM3 DX_HOSTn_OPTS="$DX_OPTS $BEAM3_DX_OPTS"
# XPOL_ONLY or YPOL_ONLY if appropriate

DXHOST1_OPTS="$DX_OPTS $BEAM1_DX_OPTS"
DXHOST2_OPTS="$DX_OPTS $BEAM1_DX_OPTS"
DXHOST3_OPTS="$DX_OPTS $BEAM1_DX_OPTS"
DXHOST4_OPTS="$DX_OPTS $BEAM2_DX_OPTS"
DXHOST5_OPTS="$DX_OPTS $BEAM2_DX_OPTS"
DXHOST6_OPTS="$DX_OPTS $BEAM2_DX_OPTS"
DXHOST7_OPTS="$DX_OPTS $BEAM3_DX_OPTS"
DXHOST8_OPTS="$DX_OPTS $BEAM3_DX_OPTS"
DXHOST9_OPTS="$DX_OPTS $BEAM3_DX_OPTS"
export DXHOST1_OPTS
export DXHOST2_OPTS
export DXHOST3_OPTS
export DXHOST4_OPTS
export DXHOST5_OPTS
export DXHOST6_OPTS
export DXHOST7_OPTS
export DXHOST8_OPTS
export DXHOST9_OPTS

RUNSSE_ZX_HOSTS="zxhost1 zxhost2 zxhost3"
export RUNSSE_ZX_HOSTS

ZXHOST1="seti000-4"
ZXHOST2="seti001-4"
ZXHOST3="seti002-4"
export ZXHOST1
export ZXHOST2
export ZXHOST3

ZXHOST1_ZX_NAMES="zx1900"
ZXHOST2_ZX_NAMES="zx2900"
ZXHOST3_ZX_NAMES="zx3900"
export ZXHOST1_ZX_NAMES
export ZXHOST2_ZX_NAMES
export ZXHOST3_ZX_NAMES

ZX_OPTS="-Z -h 8890"
export ZX_OPTS

ZXHOST1_OPTS="$DX_OPTS $BEAM1_DX_OPTS $ZX_OPTS"
ZXHOST2_OPTS="$DX_OPTS $BEAM2_DX_OPTS $ZX_OPTS"
ZXHOST3_OPTS="$DX_OPTS $BEAM3_DX_OPTS $ZX_OPTS"
export ZXHOST1_OPTS
export ZXHOST2_OPTS
export ZXHOST3_OPTS


# Backend Server Host for use with ATA
#-------------------------------------
CONTROL_COMPONENTS_ANT_CONTROL_HOST=sonata
export CONTROL_COMPONENTS_ANT_CONTROL_HOST

${HOME}/sonata_install/scripts/switchConfigFile-3beam-800KHz-69dxs-3zx.tcsh
SSE_SETUP="${HOME}/sonata_install/setup/"
export SSE_SETUP
echo $SSE_SETUP
