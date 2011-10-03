#!/bin/sh
################################################################################
#
# File:    email-jpl-ephem-xyz-requests
# Project: OpenSonATA
# Authors: The OpenSonATA code is the result of many programmers
#          over many years
#
# Copyright 2011 The SETI Institute
#
# OpenSonATA is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OpenSonATA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
# 
# Implementers of this code are requested to include the caption
# "Licensed through SETI" with a link to setiQuest.org.
# 
# For alternate licensing arrangements, please contact
# The SETI Institute at www.seti.org or setiquest.org. 
#
################################################################################


# send email requests to jpl horizons service
# for spacecraft and planet x,y,z vx,vy,vz information

#define target list

usage()
{
    echo "send email requests to jpl horizons for target x,y,z vx,vy,vz info"
    echo "usage: $0 -send"
}

if [ $# -ne 1 ]
then
    usage
    exit 1
fi

if [ "$1" != "-send" ]
then
   echo "unknown option: $1"
   usage
   exit 1
fi

# jpl target codes

stardust=-29
voyager1=-31
newhoriz=-98
deepimpact=-140
dawn=-203
rosetta=-226
mars=499
jupiter=599
saturn=699
earth=399
moon=301
pioneer10=-23
cassini=-82
kepler=-227

targetlist="$stardust $voyager1 $newhoriz $deepimpact \
 $dawn $rosetta $cassini $kepler \
 $mars $earth $jupiter $saturn $moon"

# Set up a years worth of data.
# Start with a few days ago to allow for interpolation
# so data can be used today.

# date format: yyyy-mon-dd 

dateFormat="%Y-%b-%d"
startDate=`date -u -d '3 days ago' +"$dateFormat"`
stopDate=`date -u -d '3 days ago + 1 year' +"$dateFormat"`

mailSubject="job"
mailAddr="horizons@ssd.jpl.nasa.gov"
returnAddr="$LOGNAME@seti.org"

for target in $targetlist 
do

# earth needs the sun as its center body
center='CENTER=coord'
if [ $target -eq $earth ]
then
   sunBarycenter="000"
   center="CENTER='@$sunBarycenter'"
fi

#create email request in jpl horizons format

cat <<EOF | mailx -s"$mailSubject" $mailAddr
!\$\$SOF

EMAIL_ADDR = '$returnAddr'

COMMAND='$target'
$center

TIME_ZONE = 'UT+00:00'
START_TIME = '$startDate'
STOP_TIME = '$stopDate'

STEP_SIZE= '1 day'

TABLE_TYPE = 'VECTORS'

!  2 =  State vector {x,y,z,vx,vy,vz}
!  3 =  State vector + 1-way light-time + range + range-rate 
VECT_TABLE = '3'
REF_PLANE  = 'FRAME'

! apply light time correction 
VECT_CORR  = 'LT'

! Hat Creek
SITE_COORD= '-121.473333,+40.817777,1.043'
COORD_TYPE= 'GEODETIC'

ANG_FORMAT= 'DEG'
OUT_UNITS= 'KM-S'
RANGE_UNITS= 'AU'
APPARENT= 'AIRLESS'
SKIP_DAYLT= 'NO'
EXTRA_PREC= 'YES'
R_T_S_ONLY= 'NO'
REF_SYSTEM= 'J2000'
CSV_FORMAT= 'YES'
OBJ_DATA= 'NO'
SUPPRESS_RANGE_RATE= 'NO'

!\$\$EOF
EOF

done

echo "Sent mail to $mailAddr for these targets: $targetlist"