#!/bin/sh
#############################################

isodate=`date --date=yesterday "+%F"`
echo ${isodate}
echo `date`
cp -r /home/sonata/sonata_archive/${isodate} /Sse.Arc5/sonata/sonata_archive

echo "Copied directory ${isodate} to /Sse.Arc5/sonata/sonata_archive/${isodate}"

echo `date`
cp /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt /Sse.Arc5/sonata/sonata_archive/systemlogs

echo "Copied /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt to /Sse.Arc5/sonata/sonata_archive/systemlogs"

echo `date`
#-------------------------------------------------
echo "Disk space: "
echo "------------"
df -k
echo ""

