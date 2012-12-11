#!/bin/sh
#############################################

isodate=`date --date=yesterday "+%F"`
echo ${isodate}

scp -r /home/sonata/sonata_archive/${isodate} sonata@sse300:/data/sonata/sonata_backup

echo "Copied directory ${isodate} to sse300:/data/sonata/sonata_backup"

scp /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt sonata@sse300:/data/sonata/sonata_backup/systemlogs

echo "Copied /home/sonata/sonata_archive/permlogs/systemlogs/systemlog-${isodate}.txt to sse300:/data/sonata/sonata_backup/systemlogs"

#-------------------------------------------------
echo "Disk space: "
echo "------------"
df -k
echo ""

