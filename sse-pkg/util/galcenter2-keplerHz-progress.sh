#!/bin/sh

PATH="/usr/local/bin:/usr/bin:/usr/local/mysql/bin:/bin"
export PATH

minFM=$1
maxFM=$2
MYSQL_ROOT=/usr/local/mysql
LD_LIBRARY_PATH="${MYSQL_ROOT}/lib:/usr/local/lib"
export LD_LIBRARY_PATH

dateUtc=`date -u`
echo "minFM = " $minFM "maxFM = " $maxFM
echo ""
echo "Current Time: $dateUtc"
echo ""
echo "Galactic Center Survey Progress"
echo "17 grid points"
echo "Frequency Range " $minFM "-" $maxFM "MHz"
minFMm01=`echo "$minFM - 0.1" | bc`
minFMa05=`echo "$minFM + 0.5" | bc`
maxFMm02=`echo "$maxFM - 0.2" | bc`
maxFMm07=`echo "$maxFM - 0.7" | bc`
maxFMm04=`echo "$maxFM - 0.4" | bc`

mysql -t galcenter2 << EOF
select distinct ActivityUnits.targetId, format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz, format(min(dxLowFreqMhz),1) as MinFreqMHz, format(max(dxHighFreqMhz),1) as MaxFreqMHz  from ActivityUnits, TargetCat, Activities  where ActivityUnits.targetId = TargetCat.targetId and ActivityUnits.activityId = Activities.id and ActivityUnits.validObservation = 'Yes' and Activities.type = 'target' and dxLowFreqMhz > $minFMm01 and dxHighFreqMhz < $maxFMm02 group by ActivityUnits.targetId;
EOF
mysql -t galcenter2 << EOF
create temporary Table GalSummary 
      select distinct ActivityUnits.targetId as Target, format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz, format(min(dxLowFreqMhz),1) as MinFreqMHz, format(max(dxHighFreqMhz),1) as MaxFreqMHz  from ActivityUnits, TargetCat, Activities  where ActivityUnits.targetId = TargetCat.targetId and ActivityUnits.activityId = Activities.id and ActivityUnits.validObservation = 'Yes' and Activities.type = 'target' and dxLowFreqMhz > $minFMm01 and dxHighFreqMhz < $maxFMm02 group by ActivityUnits.targetId;
#select  count(distinct Target ) as CompletedGridPoints from GalSummary where MinFreqMHz  > $minFMm01 and MinFreqMHz < $minFMa05 and MaxFreqMHz > $maxFMm07 and MaxFreqMHz < $maxFMm04;
select  count(distinct Target ) as CompletedGridPoints from GalSummary where MinFreqMHz < $minFMa05 and MaxFreqMHz < $maxFMm04;
EOF
echo ""
echo "Kepler Habitable Zone Progress"
echo "54 Target Stars"
#echo "Frequency Range 8760-8840 MHz"
echo "Frequency Range " $minFM "-" $maxFM "MHz"
#mysql -t kepler6867mhz << EOF
#mysql -t kepler7067mhz << EOF
mysql -t kepler8ghz << EOF
select ActivityUnits.targetId, format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz, format(min(dxLowFreqMhz),1) as MinFreqMHz, format(max(dxHighFreqMhz),1) as MaxFreqMHz  from ActivityUnits, TargetCat, Activities  where ActivityUnits.targetId = TargetCat.targetId and ActivityUnits.activityId = Activities.id and ActivityUnits.validObservation = 'Yes' and dxLowFreqMhz  > $minFM and dxHighFreqMhz < $maxFM and Activities.type = 'target' and TargetCat.catalog = 'keplerHz'  group by ActivityUnits.targetId;
EOF
mysql -t kepler8ghz << EOF
#create temporary table KepSummary
#select ActivityUnits.targetId as Target, format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz, format(min(dxLowFreqMhz),1) as MinFreqMHz, format(max(dxHighFreqMhz),1) as MaxFreqMHz  from ActivityUnits, TargetCat, Activities  where ActivityUnits.targetId = TargetCat.targetId and ActivityUnits.activityId = Activities.id and ActivityUnits.validObservation = 'Yes' and dxLowFreqMhz  > 8759.0 and dxHighFreqMhz < 8840.5 and Activities.type = 'target' and TargetCat.catalog = 'keplerHz'  group by ActivityUnits.targetId;

create temporary table KepSummary
select ActivityUnits.targetId as Target, format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz, format(min(dxLowFreqMhz),1) as MinFreqMHz, format(max(dxHighFreqMhz),1) as MaxFreqMHz  from ActivityUnits, TargetCat, Activities  where ActivityUnits.targetId = TargetCat.targetId and ActivityUnits.activityId = Activities.id and ActivityUnits.validObservation = 'Yes' and dxLowFreqMhz  > $minFM and dxHighFreqMhz < $maxFM and Activities.type = 'target' and TargetCat.catalog = 'keplerHz'  group by ActivityUnits.targetId;

#select count(distinct Target ) as CompletedGridPoints from KepSummary where MinFreqMHz > $minFM and and MaxFreqMHz <$maxFM 
EOF

