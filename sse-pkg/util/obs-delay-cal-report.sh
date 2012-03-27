#!/bin/sh
################################################################################
#
# File:    obs-cal-report.sh
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


# Scan yesterday & today's system logs for calibration information

PATH="${HOME}/sonata_install/bin:/usr/local/bin:/usr/bin:/bin"
export PATH

echo "Calibration report: `date`"
echo ""

logCount=0
for day in today
do
   # yyyy-mm-dd
   isoDate=`date --date=$day "+%F"`

   systemLog="${HOME}/sonata_archive/permlogs/systemlogs/systemlog-${isoDate}.txt "
   if [ -r ${systemLog} ]
   then
      echo "-------------------------------------------"
      echo "Log: ${systemLog}"
      echo "-------------------------------------------"
      cat ${systemLog} | extract-delay-cals.sh | mailx -s "SonATA Delay Cal Report" -r observing@seti.org observing@seti.org

      logCount=`expr $logCount + 1`
   fi
done

if [ $logCount -eq 0 ]
then
    echo "No system logs found."
fi
