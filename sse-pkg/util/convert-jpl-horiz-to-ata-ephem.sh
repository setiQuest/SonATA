#!/bin/sh
################################################################################
#
# File:    convert-jpl-horiz-to-ata-ephem
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


# convert ephemeris data from
# jpl horizons in this format:
#    yyyy-mm-dd hh:mm:ss (utc), <xxx>,<xxx>, ra, dec, azdeg, eldeg, ...
# to output format in ata ephem:
#    timeInNanosecs azdeg eldeg 0.0000000000E00
#
# Note: must first edit date format from Horizons, changing month
#   from abbreviation to number (eg, May -> 05)

#set -x

# extract comma separated fields
IFS=","

gnuDate="/usr/local/bin/date"

# assume infinite distance
lastAtaField="0.0000000000E00"

# convert to atomic time (TAI) (i.e., account for leap seconds)
atomOffsetSecs=34

while read dateStr foo1 foo2 ra dec azDeg elDeg remainder
do
   #Use GNU date to convert to unix timestamp (secs since epoch):
   #eg % date --date "2000-01-01 12:00 UTC" +'%s'

   timeInSecs=`$gnuDate --date "$dateStr UTC" "+%s"`

   #echo "timeInSecs = $timeInSecs"

   timeInSecs=`expr $timeInSecs + $atomOffsetSecs`

   # convert time to nanoseconds
   timeInNanoSecs="${timeInSecs}000000000"

   echo "$timeInNanoSecs $azDeg $elDeg $lastAtaField"

done


