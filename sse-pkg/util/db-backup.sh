#!/bin/sh
#############################################

isodate=`date --date=today "+%F"`
echo ${isodate}

mysqldump kepler201208 > /media/spare/sonata_backup/databases/dump-kepler201208-${isodate} 

echo "Dumped database kepler201208 to /media/spare/sonata_backup/databases/dump-kepler201208-${isodate}"

mysqldump exoplanets201208 > /media/spare/sonata_backup/databases/dump-exoplanets201208-${isodate} 

echo "Dumped database exoplanets201208 to /media/spare/sonata_backup/databases/dump-exoplanets201208-${isodate}"

#-------------------------------------------------
echo "Disk space: "
echo "------------"
df -k
echo ""

