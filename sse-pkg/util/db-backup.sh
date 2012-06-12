#!/bin/sh
#############################################

isodate=`date --date=today "+%F"`
echo ${isodate}

mysqldump kepler201205 > /Sse.Arc2/sonata_backup/databases/dump-kepler201205-${isodate} 

echo "Dumped database kepler201205 to /Sse.Arc2/sonata_backup/databases/dump-kepler201205-${isodate}"

mysqldump exoplanets201205 > /Sse.Arc2/sonata_backup/databases/dump-exoplanets201205-${isodate} 

echo "Dumped database exoplanets201205 to /Sse.Arc2/sonata_backup/databases/dump-exoplanets201205-${isodate}"
