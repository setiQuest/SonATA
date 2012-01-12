#!/bin/sh
################################################################################
#
# File:    runsse.sh
# Project: OpenSonATA
# Authors: The OpenSonATA code is the result of many programmers
#          over many years
#
# Copyright 2011 The SETI Institute
#
# OpenSonATA is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OpenSonATA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
# 
# Implementers of this code are requested to include the caption
# "Licensed through SETI" with a link to setiQuest.org.
# 
# For alternate licensing arrangements, please contact
# The SETI Institute at www.seti.org or setiquest.org. 
#
################################################################################


# Start up a bunch of xterms running
# seeker, various components, and tailed output files.

# Environmental variable overrides:
# See below.

# In (-sim) simulated mode, the env variable 
# RUNSSE_NSIMDXS can be used to override the
# number of simulated dxs that are created.

# With the (-watch) option, the seeker is 
# started under the 'screen'
# program, so that it can be monitored/controlled
# by an outside observer as needed.

# defaults for user selectable options
SIMULATOR="true"
LAB_MODE="false"
TSCOPE_SIM="true"
RUN_IFC1="true"
RUN_IFC2="true"
RUN_IFC3="false"
RUN_TSIG1="false"
RUN_TSIG2="false"
RUN_TSIG3="false"
RUN_ATASIM="false"
USE_SCREEN_SESSION="false"
INTERACTIVE="true"
DEBUG="false"
SONATA="true"
NODXS="false"
NOZXS="false"
NOCHANS="false"
NODX="true"

# process command line args
while [ "$1" != "" ]
do
   if [ "$1" = "-sim" ]
   then
      SIMULATOR="true"
   elif [ "$1" = "-nodx" ]
   then
      NODX="true"
   elif [ "$1" = "-lab" ]
   then
      LAB_MODE="true"
   elif [ "$1" = "-tscopesim" ]
   then
      TSCOPE_SIM="true"
   elif [ "$1" = "-noatasim" ]
   then
      RUN_ATASIM="false"
   elif [ "$1" = "-ifc1only" ]
   then
      RUN_IFC1="true"
      RUN_IFC2="false"
      RUN_IFC3="false"
   elif [ "$1" = "-ifc2only" ]
   then
      RUN_IFC1="false"
      RUN_IFC2="true"
      RUN_IFC3="false"
   elif [ "$1" = "-ifc3only" ]
   then
      RUN_IFC1="false"
      RUN_IFC2="false"
      RUN_IFC3="true"
   elif [ "$1" = "-noifc1" ]
   then
      RUN_IFC1="false"
   elif [ "$1" = "-noifc2" ]
   then
      RUN_IFC2="false"
   elif [ "$1" = "-noifc3" ]
   then
      RUN_IFC3="false"
   elif [ "$1" = "-tsig1only" ]
   then
      RUN_TSIG1="true"
      RUN_TSIG2="false"
      RUN_TSIG3="false"
   elif [ "$1" = "-tsig2only" ]
   then
      RUN_TSIG1="false"
      RUN_TSIG2="true"
      RUN_TSIG3="false"
   elif [ "$1" = "-tsig3only" ]
   then
      RUN_TSIG1="false"
      RUN_TSIG2="false"
      RUN_TSIG3="true"
   elif [ "$1" = "-notsig1" ]
   then
      RUN_TSIG1="false"
   elif [ "$1" = "-notsig2" ]
   then
      RUN_TSIG2="false"
   elif [ "$1" = "-notsig3" ]
   then
      RUN_TSIG3="false"
   elif [ "$1" = "-watch" ]
   then
      USE_SCREEN_SESSION="true"
   elif [ "$1" = "-batch" ]
   then
      INTERACTIVE="false"
      USE_SCREEN_SESSION="false"
   elif [ "$1" = "-debug" ]
   then
      DEBUG="true"
   elif [ "$1" = "-sonata" ]
   then
	SONATA="true"
	SIMULATOR="true"
	NODX="true"
	RUN_IFC1="true"
	RUN_IFC2="true"
	RUN_IFC3="false"
	RUN_TSIG1="false"
	RUN_TSIG2="false"
	RUN_TSIG3="false"
	TSCOPE_SIM="true"
	RUN_ATASIM="false"
   elif [ "$1" = "-nodxs" ]
   then
	NODXS="true"
   elif [ "$1" = "-nochans" ]
   then
	NOCHANS="true"
   else
      echo "Invalid argument: $1"
      echo "Usage: $0 [-sim] [-sonata] [-nodx] [-lab] [-ifc1only] [-ifc2only] [-ifc3only] [-noifc1] [-noifc2] [-noifc3] [-tsig1only] [-tsig2only] [-tsig3only] [-notsig1] [-notsig2] [-notsig3] [-batch] [-debug] [-watch] [-tscopesim] [-noatasim]"
      echo "-sim: start simulators"
      echo "-nodx: don't start dxs (real or simulated)"
      echo "-lab: run in lab mode (simulated tscope, all else real)"
      echo "-ifcXonly: use IF chain X only"
      echo "-noifcX: don't use IF chain X"
      echo "-tsigXonly: use Test Gen X only"
      echo "-notsigX: don't use Test Gen X"
      echo "-batch: run in non-interactive mode"
      echo "-debug: echo all commands as they are run"
      echo "-watch: let others watch/control the seeker via 'screen'"
      echo "-tscopesim: start telescope server in simulator mode"
      echo "-noatasim: don't start the ATA simulator"
      echo "-sonata: run SonATA configuration"
      exit
   fi
   shift

done


if [ "${DEBUG}" = "true" ]
then
    # print commands as they are executed
    set -x
fi

# define machines that host various servers
SEEKER_HOST=`hostname`

# allow various environment variable overrides 
: ${RUNSSE_NSIMDXS:=6}  

: ${CONTROL_COMPONENTS_DX_ARCH1_HOST:=${SEEKER_HOST}}
: ${CONTROL_COMPONENTS_DX_ARCH2_HOST:=${SEEKER_HOST}}
: ${CONTROL_COMPONENTS_DX_ARCH3_HOST:=${SEEKER_HOST}}

: ${RUNSSE_IFC1_DXS:=""}
: ${RUNSSE_IFC2_DXS:=""}
: ${RUNSSE_IFC3_DXS:=""}

: ${RUNSSE_FONT:="9x15"}

: ${RUNSSE_DX_HOSTS:=""}
: ${RUNSSE_CHAN_HOSTS:=""}

# Treat unset variables as an error
#set -u

cleanup()
{
#echo "calling cleanup"

    if [ "${USE_SCREEN_SESSION}" = "true" ]
    then
	# kill the 'screen' session
	screen -S ${SCREEN_SESSION_NAME} -X quit
    fi


    # first give them a chance to cleanup:
    kill -QUIT ${joblist} > /dev/null 2>&1

    # now for the sure kill
    kill -KILL ${joblist} > /dev/null 2>&1

    # seeker doesn't always die when killed 
    # via it's parent process, so kill it by name too

    pkill -KILL -x seeker

    exit 0

}


# cleanup if ctrl-c is issued, or the script exits with an error
trap cleanup INT QUIT ABRT ILL HUP KILL ALRM TERM


# define sse/seeker port numbers for components
# dx archivers
DEFAULT_DX_TO_DX_ARCH1_PORT=8857
DEFAULT_DX_TO_DX_ARCH2_PORT=8858
DEFAULT_DX_TO_DX_ARCH3_PORT=8859

# Determine the archive dir based on the SSE_ARCHIVE env var.
if [ "${SSE_ARCHIVE}" ]
then
    ARCHIVE_DIR="${SSE_ARCHIVE}"
else
    ARCHIVE_DIR="${HOME}/sonata_archive"
fi

# get the "setup" dir for config files
SETUP_DIR=`printSseSetupDir`

# define the location of the logs
TEMPLOG_DIR="${ARCHIVE_DIR}/templogs"
PERMLOG_DIR="${ARCHIVE_DIR}/permlogs"

# remove & recreate the logs so that later tails track them properly
ACTLOG="${TEMPLOG_DIR}/sse-activity-log.txt"
rm -f ${ACTLOG}
touch ${ACTLOG}

SYSTEM_STATUS_LOG="${TEMPLOG_DIR}/sse-system-status.txt"
rm -f ${SYSTEM_STATUS_LOG}
touch ${SYSTEM_STATUS_LOG}

VERBOSE_DEBUG_LOG="${TEMPLOG_DIR}/sse-debug-log.txt"
SELECTED_SYSTEMLOG_FONT="8x13"

# note: expected nss components config file is assumed to live
# in ${SSE_SETUP}
EXPECTED_COMPONENTS_FILE="expectedSonATAComponents.cfg"

# allow seeker to be run with memory checkers etc.
: ${RUNSSE_SEEKER_WRAPPER:=""}

if [ "${USE_SCREEN_SESSION}" = "true" ]
then
    # clean up old screen sessions
    screen -wipe > /dev/null 2>&1 &

    # prepare to start a screen session by name
    SCREEN_SESSION_NAME="screen-runsse"
    RUNSSE_SEEKER_WRAPPER="screen -S ${SCREEN_SESSION_NAME}"

fi

# Start a subcomponent (or process).
# In interactive mode, start each one in an xterm.
# Each xterm spawns a shell, and each executed
# program is followed by a shell, so that all
# the windows stick around until this script
# exits.  That way the windows remain up for
# debugging purposes in case any of the programs
# exit prematurely.
# Note the use of the "-t" flag with ssh, so that
# the remote process will terminate when killed on this end.

componentLogPrefix="/tmp/${LOGNAME}-runsse-log"

startComponent()
{
    compTitle=$1
    compXtermOpts=$2
    compCmd=$3

    name=`echo $compTitle | cut -f1 -d" "`

    if [ "${INTERACTIVE}" = "true" ]
    then

	# -sb = save lines and add scrollbar
	# -sl = number of lines to save
	nScrollLines=2000
	COMMON_XTERM_OPTIONS="-sb -sl ${nScrollLines}"

	xterm -title "${compTitle}" \
	${compXtermOpts} ${COMMON_XTERM_OPTIONS} -e sh \
	-c "${compCmd}; sh" &
    else
        ${compCmd} > "${componentLogPrefix}.${name}.txt" 2>&1 &
    fi

    # save the process ID so it can be killed later
    joblist="$joblist $!"
}

seekerUiOpt=""
if [ "$INTERACTIVE" = "false" ]
then
    seekerUiOpt="--noui"
fi

# start the seeker

title="SSE-Seeker"
xtermOpts="-geometry 106x17-8-84 -font ${RUNSSE_FONT}"
cmd="${RUNSSE_SEEKER_WRAPPER} seeker \
  --dx-archiver1-hostname ${CONTROL_COMPONENTS_DX_ARCH1_HOST} \
  --dx-to-dx-archiver1-port ${DEFAULT_DX_TO_DX_ARCH1_PORT} \
  --dx-archiver2-hostname ${CONTROL_COMPONENTS_DX_ARCH2_HOST} \
  --dx-to-dx-archiver2-port ${DEFAULT_DX_TO_DX_ARCH2_PORT} \
  --dx-archiver3-hostname ${CONTROL_COMPONENTS_DX_ARCH3_HOST} \
  --dx-to-dx-archiver3-port ${DEFAULT_DX_TO_DX_ARCH3_PORT} \
  --expected-components-file ${EXPECTED_COMPONENTS_FILE} \
 $seekerUiOpt" 

startComponent "$title" "$xtermOpts" "$cmd"


if [ "$INTERACTIVE" = "true" ]
then

   # start monitoring the logs

   # tail the verbose debug log, starting from the first line
   #title="Debug Log"
   #xtermOpts="-geometry 78x6+2+669 -font ${RUNSSE_FONT} -iconic"
   #cmd="tail +0f ${VERBOSE_DEBUG_LOG}"
   #startComponent "$title" "$xtermOpts" "$cmd"

   # tail the systemlogs
   title="System Log"
   xtermOpts="-geometry 89x45+27+169 -font ${RUNSSE_FONT}"
   cmd="xtail ${PERMLOG_DIR}/systemlogs"
   startComponent "$title" "$xtermOpts" "$cmd"

   # tail the systemlogs, extracting only the essential information
   title="Selected System Log Info (Tuning & Candidate Information)"
   xtermOpts="-geometry 109x15+27-83 -font ${SELECTED_SYSTEMLOG_FONT}"
   cmd="xtail ${PERMLOG_DIR}/systemlogs | sonata-extract-systemlog-info.sh"
   startComponent "$title" "$xtermOpts" "$cmd"

   # tail the errorlogs
   title="Message / Error Log"
   xtermOpts="-geometry 83x7+26+25 -font ${RUNSSE_FONT}"
   cmd="xtail ${PERMLOG_DIR}/errorlogs"
   startComponent "$title" "$xtermOpts" "$cmd"

   # tail the systemlogs, extracting only the info needed for the 
   # observers log
   title="Observer's log"
   xtermOpts="-geometry 109x15+29-20 -font ${SELECTED_SYSTEMLOG_FONT}"
   cmd="xtail ${PERMLOG_DIR}/systemlogs | sonata-extract-observers-log-info.sh"
   startComponent "$title" "$xtermOpts" "$cmd"

   # tail the system status log, starting from the first line
   title="System Status"
   xtermOpts="-geometry 125x46-0+4 -font ${RUNSSE_FONT}"
   cmd="tail +0f ${SYSTEM_STATUS_LOG}"
   startComponent "$title" "$xtermOpts" "$cmd"

   # tail the activity log, starting from the first line
   title="Activity Log"
   xtermOpts="-geometry 124x5-7+734 -font ${RUNSSE_FONT}"
   cmd="tail +0f ${ACTLOG}"
   startComponent "$title" "$xtermOpts" "$cmd"

fi 

# give seeker a chance to startup before components connect
sleep 5

# create a list of components to start based on the
# commandline options.  

componentsToStart=""

addComponentToStartList()
{
    name=$1
    componentsToStart="$componentsToStart $name"
}

# --- Start the dx data archivers ------


if [ ${RUN_IFC1} = "true" ]
then
    addComponentToStartList arch1 
fi

if [ ${RUN_IFC2} = "true" ]
then
   addComponentToStartList arch2 
fi

if [ ${RUN_IFC3} = "true" ]
then
   addComponentToStartList arch3 
fi

# --- end starting dx data archivers ------

# start the simulators

if [ "$SIMULATOR" = "true" ]
then
   if [ "$SONATA" = "true" ]
   then

  # Start the Dx Hosts
	if [ "$NODXS" = "true" ]
	then
           nop=1
        else 

           hostlist=${RUNSSE_DX_HOSTS}

           for dxhost in ${hostlist}
           do
	       addComponentToStartList $dxhost
       
           done
         fi

  # Start the Zx Hosts
	if [ "$NOZXS" = "true" ]
	then
           nop=1
        else 

           hostlist=${RUNSSE_ZX_HOSTS}

           for zxhost in ${hostlist}
           do
	       addComponentToStartList $zxhost
       
           done
         fi

  # Start the Channelizer Hosts

    if [ "$NOCHANS" = "true" ]
    then
        nop = 1
    else
        hostlist=${RUNSSE_CHAN_HOSTS}

        for chanhost in ${hostlist}
        do
	    addComponentToStartList $chanhost

        done

     fi
  fi 

  # --- simulated dxs -----
  if [ "${NODX}" ]
  then 

     nop=1  # do nothing (don't start any dxs) 

  else

    # create ndxs, starting numbering with dxid

    # starting dx ID number.  +1 for each subsequent simulator.
    dxid=1001
    dxCount=1

    # start dxs
    while [ ${dxCount} -le ${RUNSSE_NSIMDXS} ]
    do
	addComponentToStartList dxsim${dxid}

        dxid=`expr ${dxid} + 1`
        dxCount=`expr ${dxCount} + 1`

    done
	
  fi
  # --- end simulated dxs ---

  if [ ${RUN_ATASIM} = "true" ]
  then

     # start ATA telescope simulator
     title="ATA-control-server-simulator"
     xtermOpts="-geometry 79x6+4+306 -iconic"
     cmd="atacontrolsim"
     startComponent "$title" "$xtermOpts" "$cmd"
  fi

fi
# -------end simulated only mode -------


# Start components in this section that have the same
# names in both simulated and 'real' modes:

# start telescopes
# -----------------
for tscopeNumber in 1 
do
  addComponentToStartList tscope${tscopeNumber}
done

# start tsigs
# -----------------  
if [ ${RUN_TSIG1} = "true" ]
then
  addComponentToStartList tsig1
fi

if [ ${RUN_TSIG2} = "true" ]
then 
  addComponentToStartList tsig2
fi

if [ ${RUN_TSIG3} = "true" ]
then
  addComponentToStartList tsig3
fi


# start ifcs 
# -----------------
if [ ${RUN_IFC1} = "true" ]
then
  addComponentToStartList ifc1
fi

if [ ${RUN_IFC2} = "true" ]
then
  addComponentToStartList ifc2
fi

if [ ${RUN_IFC3} = "true" ]
then
  addComponentToStartList ifc3 
fi

# nonsimulated only section
if [ "$SIMULATOR" = "false" ]
then

  # -- start time broadcast ---

  SINGLE_PPS="true" 
  if [ "${SINGLE_PPS}" = "true" ]
  then 
    addComponentToStartList pps1
  else

     # start pps time broadcast on each dx net.

     if [ ${RUN_IFC1} = "true" ]
     then
       addComponentToStartList pps1
     fi

     if [ ${RUN_IFC2} = "true" ]
     then
       addComponentToStartList pps2
     fi

     if [ ${RUN_IFC3} = "true" ]
     then
       addComponentToStartList pps3
     fi
  fi

  # start real dxs
  if [ "${NODX}" ]
  then 

     nop=1  # do nothing (don't start any dxs) 

  else

    dxlist=""

    if [ ${RUN_IFC1} = "true" ]
    then
	dxlist="$dxlist ${RUNSSE_IFC1_DXS}"
    fi

    if [ ${RUN_IFC2} = "true" ]
    then
	dxlist="$dxlist ${RUNSSE_IFC2_DXS}"
    fi

    if [ ${RUN_IFC3} = "true" ]
    then
	dxlist="$dxlist ${RUNSSE_IFC3_DXS}"
    fi

    for dx in ${dxlist}
    do
        addComponentToStartList $dx
    done

  fi 

  # start real dxs
  if [ "${NODX}" ]
  then 

     nop=1  # do nothing (don't start any dxs) 

  else

    hostlist=${RUNSSE_DX_HOSTS}
    echo $hostlist

    for dxhost in ${hostname}
    do
		if ["$dxhost" = "segin"]
		then 
			for dx in ${segin_DX_NAMES}
			do
				addComponentToStartList $dx
                                echo $dx
			done

		fi
		if ["$dxhost" = "chopin"]
		then 
			for dx in ${chopin_DX_NAMES}
			do
				addComponentToStartList $dx
                                echo $dx
			done
		fi

    done

  fi 

# -- end real equipment (non simulated mode) ---
fi


# --- start the component controller
controlComponentsOpts="-autorestart"
if [ "$SIMULATOR" = "true" ]
then
    controlComponentsOpts="${controlComponentsOpts} -sim"
fi

if [ "$LAB_MODE" = "true" ]
then
    controlComponentsOpts="${controlComponentsOpts} -lab"
fi

if [ "$TSCOPE_SIM" = "true" ]
then
    controlComponentsOpts="${controlComponentsOpts} -tscopesim"
fi

title="control-components"
xtermOpts="-geometry 79x4+2+569 -font ${RUNSSE_FONT} -iconic"
cmd="${shell} controlcomponents.expect $controlComponentsOpts $componentsToStart"
startComponent "$title" "$xtermOpts" "$cmd"

echo "$cmd" > /tmp/runsse-started-jobs

echo "started jobs: $joblist" > /tmp/runsse-started-jobs

if [ "${INTERACTIVE}" = "true" ]
then

    echo "started jobs: $joblist"

    # stick around until user indicates we're done
    echo "Press <return> to exit the runsse script..."
    read cmd

    cleanup

else

    # batch mode, wait forever
    while [ true ]
    do
	sleep 15
    done

fi
