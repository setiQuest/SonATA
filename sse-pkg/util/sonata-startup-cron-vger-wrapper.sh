#!/bin/sh
################################################################################
#
# File:    sonata-startup-cron-wrapper.sh
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


# sonata-startup-cron-wrapper

# start the sse in batch mode (automated operations) from cron
# all arguments are passed through to the runsse script

ulimit -s unlimited

GCC_ROOT=/usr/local/
ACE_ROOT=/usr/local/ACE_wrappers
MYSQL_ROOT=/usr/local/mysql
SSE_ROOT=${HOME}/sonata_install
SSE_SETUP=${HOME}/sonata_install/setup

# print commands as they are executed
#set -x

PATH=${GCC_ROOT}/bin:/usr/local/sbin:/usr/local/java/bin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin:/usr/ccs/bin:/usr/ucb:${SSE_ROOT}/bin
export PATH

# Set up the shell environment:
# -u    Treat unset variables as an error when substituting.
set -u

LD_LIBRARY_PATH=${GCC_ROOT}/lib:/usr/local/lib:${ACE_ROOT}/ace
export LD_LIBRARY_PATH

# override the SSE startup file (use the one customized for automated mode)
SSE_INITRC="${HOME}/sonata_install/scripts/sserc-auto.tcl"
export SSE_INITRC

if [ ! -r ${SSE_INITRC} ]
then
    echo "*** Error: `basename $0`: required startup file not found: $SSE_INITRC"
    exit 1
fi

# load any env var overrides
#envFile="${HOME}/sonata_install/scripts/3beam-dualpol-800KHz-72dx-env-vars-batch.sh"
envFile="${HOME}/sonata_install/scripts/1beam-beam3-dualpol-800KHz-9dx-1zx-256F-env-vars-batch.sh"

if [ -r ${envFile} ]
then
   . ${envFile}
fi

cd ${HOME}/sonata_install/scripts

runsse.sh $* &

exit 0
