#!/bin/sh
################################################################################
#
# File:    convert-jpl-xyz-data
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


# Convert jpl position data to prelude .xyz file format.
# Data region starts with SOE and ends with EOF
# Input data lines look like this:
# jd-date, date, x, y, z, vx, vy, vz, lightTime, range, rangeRate

gawk '
BEGIN {
   insideData=0
   dataRowCount=0
   maxDataRows=366
}

# found end of data
/\$\$EOE/ { insideData=0 }


/Target body name/ { target=$0 }
/Start/ { startTime=$0 }
/Stop/ { stopTime=$0 }

{ 
   if (insideData == 1)
   { 
      dataRowCount += 1
      if (dataRowCount < maxDataRows)
      {
         # split on commas
         split($0, fields, ",")

         # print fields, ignoring extra date info in field 2
         printf "%s %s %s %s %s %s %s %s %s %s\n",fields[1],fields[3],fields[4],fields[5],fields[6],fields[7],fields[8],fields[9],fields[10],fields[11]
      }

   }
}

# found start of data
/\$\$SOE/ {
 printHeader()
 insideData=1 
}


function printHeader() {
# must be 9 lines long

   print target
   print "Frame: J2000"
   print "Corrected: LT"
   print startTime
   print stopTime
   print ""
   print ""
   print "Julian Date         X (km)         Y (km)         Z (km)     Vx (km/s)  Vy (km/s)  Vz (km/s) LT(sec) Range(km) RRate(km/s)"
   print "================== ============== ============== ============== ========== ========== ========== ========== ========== ==========   "

}
'
