#!/bin/sh

PATH="/usr/local/bin:/usr/bin:/usr/local/mysql/bin:/bin"
export PATH

MYSQL_ROOT=/usr/local/mysql
LD_LIBRARY_PATH="${MYSQL_ROOT}/lib:/usr/local/lib"
export LD_LIBRARY_PATH

dateUtc=`date -u`
echo ""
echo "Current Time: $dateUtc"
echo ""
echo "Galactic Center Survey Progress"
echo "19 grid points"
echo "Frequency Range 6767-6967 MHz"
mysql -t galcenter << EOF
select ActivityUnits.targetId, format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz, format(min(dxLowFreqMhz),1) as MinFreqMHz, format(max(dxHighFreqMhz),1) as MaxFreqMHz  from ActivityUnits, TargetCat, Activities  where ActivityUnits.targetId = TargetCat.targetId and ActivityUnits.activityId = Activities.id and ActivityUnits.validObservation = 'Yes' and Activities.type = 'target' and dxLowFreqMhz > 6766.9 group by ActivityUnits.targetId;
EOF
mysql -t galcenter << EOF
select  count(distinct activityId, targetId) as CompletedGridPoints from ActivityUnits, Activities where ActivityUnits.activityId = Activities.id and dxHighFreqMhz  > 6966.3 and Activities.type = 'target' and ActivityUnits.validObservation = 'Yes' and targetId > 31999 and targetId < 33000;
EOF
echo ""
echo "Kepler Habitable Zone Progress"
echo "54 Target Stars"
echo "Frequency Range 6767-6967 MHz"
mysql -t kepler6867mhz << EOF
select ActivityUnits.targetId, format(sum(dxHighFreqMhz - dxLowFreqMhz),1) as TotalMHz, format(min(dxLowFreqMhz),1) as MinFreqMHz, format(max(dxHighFreqMhz),1) as MaxFreqMHz  from ActivityUnits, TargetCat, Activities  where ActivityUnits.targetId = TargetCat.targetId and ActivityUnits.activityId = Activities.id and ActivityUnits.validObservation = 'Yes' and Activities.type = 'target' and TargetCat.catalog = 'keplerHz'  group by ActivityUnits.targetId;
EOF
mysql -t kepler6867mhz << EOF
select  count(distinct activityId, targetId) as CompletedKeplerHzStars from ActivityUnits, Activities where ActivityUnits.activityId = Activities.id and dxHighFreqMhz  > 6966.3 and Activities.type = 'target' and ActivityUnits.validObservation = 'Yes' and targetId > 149999 and targetId < 160000;
EOF
