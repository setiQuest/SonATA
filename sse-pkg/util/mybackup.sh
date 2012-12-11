#!/bin/sh
#############################################

isodate=`date --date=yesterday "+%F"`
echo ${isodate}
echo `date`
cp -r /home/sonata/sonata_archive/${isodate} /nas-archive/sse100/2012

echo "Copied directory ${isodate} to /nas-archive/sse100/2012"

echo `date`
cp /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt /nas-archive/sse100/systemlogs

echo "Copied /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt to /nas-archive/sse100/systemlogs"

echo `date`
#-------------------------------------------------
echo "Disk space: "
echo "------------"
df -k
echo ""

