#!/bin/sh
#############################################

isodate=`date --date=yesterday "+%F"`
echo ${isodate}

cp -r /home/sonata/sonata_archive/${isodate} /Sse.Arc4/sonata_backup

echo "Copied directory ${isodate} to /Sse.Arc4/sonata_backup"

cp /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt /Sse.Arc4/sonata_backup/systemlogs

echo "Copied /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt to /Sse.Arc4/sonata_backup/systemlogs"
