#!/bin/sh
################################################################################
#
# File:    obs-summary-report.sh
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


# NSS daily obs summary queries

PATH="/usr/local/bin:/usr/bin:/usr/local/mysql/bin:/bin"
export PATH

MYSQL_ROOT=/usr/local/mysql
LD_LIBRARY_PATH="${MYSQL_ROOT}/lib:/usr/local/lib"
export LD_LIBRARY_PATH

dbHost="sse300"
dbName="waterhole2x"
hoursInterval="24"

dbHostArgName="-dbhost"
dbNameArgName="-dbname"
hoursIntervalArgName="-hours"

# process command line args
while [ "$1" ]
do
    if [ "$1" = $dbHostArgName ]
    then
	if [ $# -lt 2 ]
	then
	    echo "missing argument for $dbHostArgName"
	    exit 1
	fi

	shift
	dbHost="$1"

    elif [ "$1" = $dbNameArgName ]
    then
	if [ $# -lt 2 ]
	then
	    echo "missing argument for $dbNameArgName"
	    exit 1
	fi
	shift
	dbName="$1"
    elif [ "$1" = $hoursIntervalArgName ]
    then
	if [ $# -lt 2 ]
	then
	    echo "missing argument for $hoursIntervalArgName"
	    exit 1
	fi

	shift
	hoursInterval="$1"
    else
	echo "Invalid argument: $1"
	echo "usage: $0 [$dbHostArgName <database host>] [$dbNameArgName <database name>] [$hoursIntervalArgName <reporting interval in hours, default=$hoursInterval>]"
	exit
    fi
    shift
done


mysqlSkipColsStart="mysql --skip-column-names -h ${dbHost} ${dbName}"
mysqlStart="mysql -t -h ${dbHost} ${dbName}"

echo ""
echo "SonATA observing summary:"
echo "=========================="

host=`hostname`

echo "Reporting Host: $host"
echo "Database Host: $dbHost" 
echo "Database Name:" $dbName
echo "Reporting Interval: $hoursInterval hours"

dateUtc=`date -u`
echo "Current Time: $dateUtc"
echo ""

sqlStartTime="date_sub(NOW(), interval $hoursInterval hour)"

#-------------------------------------------------
echo "Report period: "
${mysqlStart} << EOF 
select $sqlStartTime as Start, NOW() as End;
EOF

echo ""
#-------------------------------------------------------
echo "SetiLive Candidate Signals"
${mysqlStart} << EOF 
select 
activityId as ActId, 
Activities.type as ActType,  
targetId as Target, 
dxNumber as Zx, subchanNumber as SubChan, 
startOfDataCollection as Start, 
unix_timestamp(ZxCandidateSignals.ts)-unix_timestamp(startOfDataCollection)-266 as SecLate, 
format(rfFreq,6) as Freq, format(drift,3) as Drift, reason as Reason
from ZxCandidateSignals, Activities
where ZxCandidateSignals.activityId = Activities.id
and startOfDataCollection >= $sqlStartTime
EOF


echo ""

