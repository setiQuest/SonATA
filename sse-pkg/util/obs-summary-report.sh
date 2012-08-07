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

dbHost="sse100"
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


#-------------------------------------------------
echo "Candidate Resolution Counts:"

${mysqlStart} << EOF 

select type as ActType,
sum(ActivityUnits.cwSignals + ActivityUnits.pulseSignals) as Signals,
sum(ActivityUnits.allCwCandidates + ActivityUnits.allPulseCandidates) as Cand,
sum(ActivityUnits.confirmedCwCandidates +
ActivityUnits.confirmedPulseCandidates) as Confirmed
from ActivityUnits, Activities
where ActivityUnits.activityId = Activities.id
and ActivityUnits.validObservation = 'Yes'
and type <> 'rfiscan'
and ActivityUnits.startOfDataCollection >= $sqlStartTime
group by type
order by Confirmed desc

EOF


echo ""

#-------------------------------------------------
echo "Confirmed Candidates From target5-on-nofollowup:"

${mysqlStart} << EOF

select activityId as ActId,
activityStartTime as 'StartTime (UTC) ',
Activities.type as 'ActType', 
targetId as TargetId, 
beamNumber as Beam, 
dxNumber as DX, 
signalIdNumber as SigId, 
CandidateSignals.type as Type,
rfFreq as 'RF Freq MHz',
format(power,1) as Power, 
format(drift,3) as Drift, 
subchanNumber as Subb, 
pol as Pol, 
sigClass as Class, reason as Reason, 
pfa as PFA, format(snr,3) as SNR 
from CandidateSignals, Activities 
where (reason='Confrm' or reason='RConfrm' or reason='NtSnOff') 
and (Activities.id = activityId) 
and (activityStartTime >= $sqlStartTime)
and (Activities.type = 'target5-on-nofollowup');

EOF

echo ""

#-------------------------------------------------
echo "Total number of activities:"

${mysqlStart} << EOF 

select count(*) as Count from Activities
where ts >= $sqlStartTime;

EOF

echo ""

#-------------------------------------------------
echo "Total activities by type:" 

${mysqlStart} << EOF

select distinct type as ActivityType, count(*) as ActCount from Activities
where ts >= $sqlStartTime group by type;

EOF

echo ""


#-------------------------------------------------
echo "Min/Max Act Id For This Period:"

${mysqlStart} << EOF 

select min(id), min(ts) as minTime,
max(id), max(ts) as maxTime from Activities 
where ts >= $sqlStartTime;

EOF

echo ""

#-------------------------------------------------
echo "Dxs in use:"

${mysqlStart} << EOF 

select dxNumber from ActivityUnits 
where ts >= $sqlStartTime
group by dxNumber;

EOF

echo ""


#-------------------------------------------------
echo "Total number of targets observed:"

${mysqlStart} << EOF 

select count(distinct targetId) as TargetCount from ActivityUnits 
where ActivityUnits.startOfDataCollection >= $sqlStartTime
and ActivityUnits.validObservation = 'Yes';

EOF

echo ""

#-------------------------------------------------
echo "Targets observed:"

${mysqlStart} << EOF 

select ActivityUnits.targetId,
format(TargetCat.ra2000Hours,6) as RA_Hours,
format(TargetCat.dec2000Deg,6) as Dec_Deg,
TargetCat.catalog,
TargetCat.aliases
from ActivityUnits, TargetCat 
where
ActivityUnits.startOfDataCollection >= $sqlStartTime
and ActivityUnits.targetId = TargetCat.targetId
and ActivityUnits.validObservation = 'Yes'
group by ActivityUnits.targetId;

EOF

echo ""

#-------------------------------------------------
echo "Target frequency coverage:"

# don't count followups, only initial observations

${mysqlStart} << EOF 

select ActivityUnits.targetId,
format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz,
format(min(dxLowFreqMhz),1) as MinFreqMHz,
format(max(dxHighFreqMhz),1) as MaxFreqMHz 
from ActivityUnits, TargetCat, Activities 
where
ActivityUnits.startOfDataCollection >= $sqlStartTime
and ActivityUnits.targetId = TargetCat.targetId
and ActivityUnits.activityId = Activities.id
and ActivityUnits.validObservation = 'Yes'
and Activities.type = 'target'
group by ActivityUnits.targetId;

EOF

echo ""


#------------------------------------------------
echo "Total signals detected:"

${mysqlStart} << EOF
select count(*) as 'Total Signals' from Signals
where ts >= $sqlStartTime;
EOF
echo ""


#------------------------------------------------
echo "Signal Classification Reasons:"

${mysqlStart} << EOF
select reason, count(reason) from Signals  
where ts >= $sqlStartTime
group by reason;
EOF
echo ""


#------------------------------------------------
echo "Total candidates:"

# exclude CwP (Cw power) reports so that those 
# signals don't get counted twice

${mysqlStart} << EOF
select count(*) as 'Total Candidates' from CandidateSignals
where (type='CwC' or type='Pul')
and (ts >= $sqlStartTime);
EOF
echo ""


#------------------------------------------------
echo "Candidate Signal Classification Reasons:"

${mysqlStart} << EOF
select reason, count(reason) from CandidateSignals  
where ts >= $sqlStartTime
group by reason;
EOF
echo ""



#-------------------------------------------------
echo "Signal counts per frequency band:"

${mysqlStart} << EOF

select FORMAT(minDxSkyFreqMhz,1) as MinDxFreq,
FORMAT(maxDxSkyFreqMhz,1) as MaxDxFreq,
type as ActType, count(*) as ActCount,
sum(Activities.cwSignals + Activities.pulseSignals) as Sig,
sum(Activities.allCwCandidates + Activities.allPulseCandidates) as Cand,
sum(Activities.confirmedCwCandidates + Activities.confirmedPulseCandidates)
as Confirm
from Activities
where 
Activities.startOfDataCollection >= $sqlStartTime
and Activities.validObservation = 'Yes'
group by minDxSkyFreqMhz, type;

EOF

echo ""

#-------------------------------------------------
echo "Signal types for candidates:"

${mysqlStart} << EOF

select Activities.type as ActivityType, sigClass as SignalClass, 
CandidateSignals.type as SigType, reason as SigClassReason, 
count(distinct Activities.id,rfFreq,CandidateSignals.type ) as Count
from CandidateSignals, Activities where 
Activities.id = CandidateSignals.activityId and 
Activities.startOfDataCollection >= $sqlStartTime
group by Activities.type, sigClass, reason, SigType 
order by Activities.type;

EOF

echo ""

#-------------------------------------------------
echo "Signal types for Zx candidates:"

${mysqlStart} << EOF

select Activities.type as ActivityType, sigClass as SignalClass, 
ZxCandidateSignals.type as SigType, reason as SigClassReason, 
count(distinct Activities.id,rfFreq,ZxCandidateSignals.type ) as Count
from ZxCandidateSignals, Activities where 
Activities.id = ZxCandidateSignals.activityId and 
Activities.startOfDataCollection >= $sqlStartTime
group by Activities.type, sigClass, reason, SigType 
order by Activities.type;

EOF

echo ""

#-------------------------------------------------
#echo "Signal types for all signals:"

#${mysqlStart} << EOF

#select Activities.type as ActivityType, sigClass as SignalClass,
#Signals.type as SigType, reason as SigClassReason, 
#count(distinct Activities.id,rfFreq,Signals.type ) as Count from
#Signals, Activities where Activities.id = Signals.activityId and
#Activities.startOfDataCollection >= $sqlStartTime
#group by Activities.type, sigClass,reason, SigType
#order by Activities.type;

#EOF

#echo ""

#-------------------------------------------------
#echo "Confirmed Candidates:"

#${mysqlStart} << EOF
#select activityId, dxNumber, type, rfFreq, drift, pol from 
#CandidateSignals where reason = 'Confrm' and activityStartTime >= 
#$sqlStartTime;

#EOF

#echo ""

#-------------------------------------------------
echo "Signal counts by activity:"

${mysqlStart} << EOF

select Activities.id as ActId, 
Activities.type as Type, 
minDxSkyFreqMhz as MinMhz, maxDxSkyFreqMhz as MaxMhz,
cwSignals + pulseSignals as 'TotSig',
allCwCandidates + allPulseCandidates as 'TotCand', 
confirmedCwCandidates + confirmedPulseCandidates as TotConf 
from Activities where 
Activities.startOfDataCollection >= $sqlStartTime;

EOF

echo ""

#-------------------------------------------------
echo "'Test signals' per activity (if any):"

${mysqlStart} << EOF

# count the number of test signals in each activity
# separate from rfi with a drift range and minimum signal width

select activityId, count(*) as count from CandidateSignals where 
sigClass = 'Cand' and drift >= 0.08 and drift <= 0.12 and 
width > 1.0 and activityStartTime >= $sqlStartTime
and rfFreq > 1420.8000 and rfFreq < 1420.8002
group by activityId order by count;

EOF

echo ""

#-------------------------------------------------
echo "Failed activities (if any):"

${mysqlStart} << EOF

select id as actId, ts as time, type as Type, comments from Activities where 
validObservation = 'No' and ts >= 
$sqlStartTime;

EOF

echo ""

#-------------------------------------------------
echo "Failed activity units (if any):"

${mysqlStart} << EOF

select activityId as actId, ts as time, dxNumber, comments from ActivityUnits where 
validObservation = 'No' and ts >= $sqlStartTime;

EOF

echo ""

#-------------------------------------------------
echo "Disk space: "
echo "------------"
df -k
echo ""
