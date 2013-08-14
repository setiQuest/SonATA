#!/bin/sh
#############################################

isodate=`date --date=today "+%F"`
echo ${isodate}
echo `date`
#mysqldump -h sse300 kepler201208 > /nas-archive/sse100/databases/dump-kepler201208-${isodate} 

#echo "Dumped database kepler201208 to /nas-archive/sse100/databases/dump-kepler201208-${isodate}"
#echo `date`

#mysqldump -h sse300 exoplanets201208 > /nas-archive/sse100/databases/dump-exoplanets201208-${isodate} 

#echo "Dumped database exoplanets201208 to /nas-archive/sse100/databases/dump-exoplanets201208-${isodate}"

#echo `date`
#mysqldump -h sse100 kepler6667mhz > /nas-archive/sse100/databases/dump-kepler6667mhz-${isodate} 

#echo "Dumped database kepler6667mhz to /nas-archive/sse100/databases/dump-kepler6667mhz-${isodate}"
#echo `date`

#mysqldump -h sse100 exoplanets6667mhz > /nas-archive/sse100/databases/dump-exoplanets6667mhz-${isodate} 

#echo "Dumped database exoplanets6667mhz to /nas-archive/sse100/databases/dump-exoplanets6667mhz-${isodate}"

#echo `date`
#mysqldump -h sse100 kepler6867mhz > /nas-archive/sse100/databases/dump-kepler6867mhz-${isodate} 

#echo "Dumped database kepler6867mhz to /nas-archive/sse100/databases/dump-kepler6867mhz-${isodate}"
#echo `date`

#mysqldump -h sse100 exoplanets6867mhz > /nas-archive/sse100/databases/dump-exoplanets6867mhz-${isodate} 

#echo "Dumped database exoplanets6867mhz to /nas-archive/sse100/databases/dump-exoplanets6867mhz-${isodate}"

#echo `date`
#mysqldump -h sse300 spacecraft > /nas-archive/sse100/databases/dump-spacecraft-${isodate} 

#echo "Dumped database spacecraft to /nas-archive/sse100/databases/dump-spacecraft-${isodate}"

#echo `date`
#mysqldump -h sse100 casa > /nas-archive/sse100/databases/dump-casa-${isodate} 

#echo "Dumped database casa to /nas-archive/sse100/databases/dump-casa-${isodate}"
mysqldump -h sse100 kepler8ghz > /nas-archive/sse100/databases/dump-kepler8ghz-${isodate} 

echo "Dumped database kepler8ghz to /nas-archive/sse100/databases/dump-kepler8ghz-${isodate}"
echo `date`

mysqldump -h sse100 exoplanets8ghz > /nas-archive/sse100/databases/dump-exoplanets8ghz-${isodate} 

echo "Dumped database exoplanets8ghz to /nas-archive/sse100/databases/dump-exoplanets8ghz-${isodate}"


echo `date`
mysqldump -h sse100 galcenter2 > /nas-archive/sse100/databases/dump-galcenter2-${isodate} 

echo "Dumped database galcenter2 to /nas-archive/sse100/databases/dump-galcenter2-${isodate}"


echo `date`
#-------------------------------------------------
echo "Disk space: "
echo "------------"
df -k
echo ""

