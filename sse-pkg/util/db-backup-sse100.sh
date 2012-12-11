#!/bin/sh
#############################################

isodate=`date --date=today "+%F"`
echo ${isodate}
echo `date`
mysqldump -h sse300 kepler201208 > /Sse.Arc5/sonata/sonata_archive/databases/dump-kepler201208-${isodate}

echo "Dumped database kepler201208 to /Sse.Arc5/sonata/sonata_archive/databases/dump-kepler201208-${isodate}"

echo `date`
mysqldump -h sse300 exoplanets201208 > /Sse.Arc5/sonata/sonata_archive/databases/dump-exoplanets201208-${isodate}

echo "Dumped database exoplanets201208 to /Sse.Arc5/sonata/sonata_archive/databases/dump-exoplanets201208-${isodate}"

echo `date`
mysqldump -h sse300 spacecraft > /Sse.Arc5/sonata/sonata_archive/databases/dump-spacecraft-${isodate}

echo "Dumped database spacecraft to /Sse.Arc5/sonata/sonata_archive/databases/spacecraft-${isodate}"

echo `date`
#-------------------------------------------------
echo "Disk space: "
echo "------------"
df -k
echo ""


