#!/bin/sh
################################################################################
#
# File:    daily-error-summary.sh
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


countErrors()
{
cd ${HOME}/sonata_archive/permlogs/systemlogs
echo " "
echo "********************************************"
echo "Daily SonATA Error Summary $1"
echo "********************************************"
echo "Total Activities"
echo "----------------"
echo `grep "completed successfully" *$1* | wc -l`
echo " "
echo "Fatal system error -- Observing stops"
echo "-------------------------------------"
echo "Stop received internally because Activity not found in table"
echo `grep "[1161] Activity not found" *$1* | wc -l`
echo "Lost Target Tracking"
echo `grep "Activity failed: lost target tracking" *$1* | wc -l`
echo "Timed out waiting for Telescope Ready"
echo `grep "timed out waiting for tscope ready" *$1* | wc -l`
echo "No targets remain after Culling by min target separation"
echo `grep "no targets remain" *$1* | wc -l`
echo "No targets are available within the observing constraints"
echo `grep "No targets are available" *$1* | wc -l `
echo "Dx failures"
echo "-----------"
echo "Activities with Dx Failures"
echo `grep "failed out of" *$1* | wc -l`
#echo " "
echo "DX Segmentation Faults"
echo `grep "Segment" *$1* | grep dx | wc -l`
#echo " "
echo "Channelizer Segmentation Faults"
echo `grep "Segment" *$1* | grep chan | wc -l`
echo "Channelizer ChannelPacketVector Errors"
echo `grep "ChannelPacketVector" *$1* | grep chan | wc -l`
#echo " "
echo "Channelizer Fatal Error"
echo `grep "Fatal error Beam" *$1* | grep chan | wc -l`
#echo " "
echo "Memory Allocation Errors"
echo `grep "allocation size" *$1* | wc -l`
#echo " "
echo "R&L packet streams unsynchronized"
echo `grep "R&L packet streams are unsyn" *$1* | grep  *Failed* | wc -l`
#echo " "
echo "Signal not a candidate: Secondary Confirmation"
echo `grep "signal is not a candidate" *$1* | wc -l`
#echo " "
echo "Warning: No packets received by start of baseline accumulation"
echo `grep "restarting" *$1* | wc -l`
#echo " "
echo "Error: No packets received by start of data collection"
echo `grep "data collection error" *$1* | grep *Failed*| wc -l`
#echo "  "
echo "Start time already past"
echo `grep "start time already past" *$1* | grep *Failed* | wc -l`
#echo " "
echo "Socket reset by SSE after timeout -- may include more than one Dx"
echo `grep "reset socket on these dxs: dx" *$1* | wc -l`
#echo " "
echo "Unexpected disconnect = SegFaults + Unknown Error + Memory Allocation Error"
echo " + Reset Sockets the occur at the end of the previous Activity"
echo " The reset sockets that occur at the end of Data Collection"
echo " do not cause the unexpectedly disconnected error"
echo `grep "unexpect" *$1* | grep Failed | wc -l`
#echo " "
echo "Failed Dxs = Unknown Unexpected Disconnects + No Packets Error + Memory Allocation Errors + Segmentation Faults + Start time already past"
echo `grep "*Failed*" *$1* | wc -l`
echo " "
echo "Baseline Warnings and Errors"
echo "----------------------------"
echo "Baseline warning limits exceeded: mean out of range"
echo `grep "Baseline warning limits exceeded" *$1* | grep "mean" | wc -l`
#echo " "
echo "Baseline error limits exceeded: mean out of range"
echo `grep "Baseline Error limits exceeded" *$1* | grep "mean" | wc -l`
#echo " "
echo "Baseline warning limits exceeded: range too large"
echo `grep "Baseline warning limits exceeded" *$1* | grep "large" |  wc -l`
echo "Beam1"
echo `grep "Baseline warning limits exceeded" *$1* | grep "large" | grep dx1 | wc -l`
echo "Beam2"
echo `grep "Baseline warning limits exceeded" *$1* | grep "large" | grep dx2 |  wc -l`
echo "Beam3"
echo `grep "Baseline warning limits exceeded" *$1* | grep "large" | grep dx3 | wc -l`
#echo " "
echo "Database error: Unknown column nan (zero baselines)"
echo `grep "MySQL error: Unknown column" *$1* | wc -l `

}
for day in today yesterday 
do
	#yyyy-mm-dd
	isodate=`date --date=$day "+%F"`
        countErrors ${isodate}
done
