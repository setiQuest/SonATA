/*******************************************************************************

 File:    RecordDxInfoInDb.cpp
 Project: OpenSonATA
 Authors: The OpenSonATA code is the result of many programmers
          over many years

 Copyright 2011 The SETI Institute

 OpenSonATA is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 OpenSonATA is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
 
 Implementers of this code are requested to include the caption
 "Licensed through SETI" with a link to setiQuest.org.
 
 For alternate licensing arrangements, please contact
 The SETI Institute at www.seti.org or setiquest.org. 

*******************************************************************************/



#include "RecordDxInfoInDb.h"
#include "SseDxMsg.h"
#include "SseUtil.h"
#include <sstream>

static const int PrintPrecision(9);  // MilliHz

using namespace std;

static string BoolToQuotedOnOff(bool value)
{
   if (value)
   {
      return "'on'";
   }
   else
   {
      return "'off'";
   }
}


RecordDxScienceDataRequest::RecordDxScienceDataRequest(
   const string & tableName,
   const DxScienceDataRequest& scienceDataRequest):
   RecordInDatabase(tableName),
   scienceDataRequest_(scienceDataRequest)
{
}

string RecordDxScienceDataRequest::stmt(MYSQL* db,
					 const string & conjunction)
{
   stringstream sqlstmt;

   sqlstmt << "sendBaselines = " 
	   << BoolToQuotedOnOff(scienceDataRequest_.sendBaselines)
	   << " " << conjunction << " "

	   << "sendBaselineStatistics = " 
	   << BoolToQuotedOnOff(scienceDataRequest_.sendBaselineStatistics)
	   << " " << conjunction << " "

	   << "checkBaselineWarningLimits = " 
	   << BoolToQuotedOnOff(scienceDataRequest_.checkBaselineWarningLimits)
	   << " " << conjunction << " "

	   << "checkBaselineErrorLimits = " 
	   << BoolToQuotedOnOff(scienceDataRequest_.checkBaselineErrorLimits)
	   << " " << conjunction << " "

	   << "baselineReportingHalfFrames = "
	   << scienceDataRequest_.baselineReportingHalfFrames
	   << " " << conjunction << " "

	   << "sendComplexAmplitudes = "
	   <<  BoolToQuotedOnOff(scienceDataRequest_.sendComplexAmplitudes)
	   << " " << conjunction << " "

	   << "requestType = '"
	   << SseDxMsg::sciDataRequestTypeToString(
	      scienceDataRequest_.scienceData.requestType)
	   << "'"
	   << " " << conjunction << " "

	   << "subchan = " << scienceDataRequest_.scienceData.subchannel
	   << " " << conjunction << " ";

   sqlstmt.precision(PrintPrecision);
   sqlstmt.setf(std::ios::fixed);  // show all decimal places up to precision

   sqlstmt << "rfFrequency = " 
	   << scienceDataRequest_.scienceData.rfFreq;

   return sqlstmt.str();
}

RecordDxActivityParameters::RecordDxActivityParameters(
   const string & tableName,
   const DxActivityParameters& dxActParam):
   RecordInDatabase(tableName),
   dxActParam_(dxActParam)
{
}

string RecordDxActivityParameters::stmt(MYSQL* db,
					 const string & conjunction)
{
   stringstream sqlstmt;

   sqlstmt.precision(PrintPrecision);
   sqlstmt.setf(std::ios::fixed);  // show all decimal places up to precision

   sqlstmt << "dataCollectionLength = " 
	   << dxActParam_.dataCollectionLength
	   << " " << conjunction << " "

	   << "rcvrSkyFrequency = " << dxActParam_.rcvrSkyFreq
	   << " " << conjunction << " "

	   << "ifcSkyFrequency = " << dxActParam_.ifcSkyFreq
	   << " " << conjunction << " "
      
	   << "dxSkyFrequency = " << dxActParam_.dxSkyFreq
	   << " " << conjunction << " "
      
	   << "channelNumber = " << dxActParam_.channelNumber
	   << " " << conjunction << " "

	   << "operations = " << dxActParam_.operations
	   << " " << conjunction << " "

	   << "sensitivityRatio = " << dxActParam_.sensitivityRatio
	   << " " << conjunction << " "

	   << "maxNumberOfCandidates = " << dxActParam_.maxNumberOfCandidates
	   << " " << conjunction << " "

	   << "clusteringFreqTolerance = " << dxActParam_.clusteringFreqTolerance
	   << " " << conjunction << " "

	   << "zeroDriftTolerance = " << dxActParam_.zeroDriftTolerance
	   << " " << conjunction << " "

	   << "cwClusteringDeltaFreq = " << dxActParam_.cwClusteringDeltaFreq
	   << " " << conjunction << " "

	   << "badBandCwPathLimit = " << dxActParam_.badBandCwPathLimit
	   << " " << conjunction << " "

	   << "daddResolution = '"
	   << SseDxMsg::resolutionToString(dxActParam_.daddResolution)
	   << "'"
	   << " " << conjunction << " "

	   << "daddThreshold = "  << dxActParam_.daddThreshold
	   << " " << conjunction << " "

	   << "cwCoherentThreshold = " << dxActParam_.cwCoherentThreshold
	   << " " << conjunction << " "

	   << "secondaryCwCoherentThreshold = "
	   << dxActParam_.secondaryCwCoherentThreshold
	   << " " << conjunction << " "
      
	   << "limitsForCoherentDetection = "
	   << dxActParam_.limitsForCoherentDetection
	   << " " << conjunction << " "

	   << "pulseClusteringDeltaFreq = "
	   << dxActParam_.pulseClusteringDeltaFreq
	   << " " << conjunction << " "

	   << "badBandPulseTripletLimit = "
	   << dxActParam_.badBandPulseTripletLimit
	   << " " << conjunction << " "

	   << "pulseTrainSignifThresh = "
	   << dxActParam_.pulseTrainSignifThresh
	   << " " << conjunction << " "

	   << "secondaryPulseTrainSignifThresh = "
	   << dxActParam_.secondaryPulseTrainSignifThresh
	   << " " << conjunction << " "

	   << "badBandPulseLimit = "
	   << dxActParam_.badBandPulseLimit
	   << " " << conjunction << " "

	   << "maxPulsesPerHalfFrame = "
	   << dxActParam_.maxPulsesPerHalfFrame
	   << " " << conjunction << " "

	   << "maxPulsesPerSubchannelPerHalfFrame = "
	   << dxActParam_.maxPulsesPerSubchannelPerHalfFrame
	   << " ";

#if 0

   // TBD how to handle this in the table

   sqlstmt << " " << conjunction << " ";

   sqlstmt << "requestPulseResolution = "
	   << dxActParam_.requestPulseResolution;

   sqlstmt << " " << conjunction << " ";

   sqlstmt << "pd = "
	   << dxActParam_.pd;

#endif

   RecordDxScienceDataRequest 
      recordDxScienceDataRequest("DxScienceDataRequest",
				  dxActParam_.scienceDataRequest);

   sqlstmt << " " << conjunction << " "
	   << "dxScienceDataRequestId = "
	   << recordDxScienceDataRequest.record(db)
	   << " " << conjunction << " "

	   << "baselineSubchannelAverage = "
	   << dxActParam_.baselineSubchannelAverage
	   << " " << conjunction << " "

	   << "baselineInitAccumHalfFrames = "
	   << dxActParam_.baselineInitAccumHalfFrames
	   << " " << conjunction << " "

	   << "baselineDecay = "
	   << dxActParam_.baselineDecay
	   << " " << conjunction << " ";

   RecordBaselineLimits 
      recordBaselineWarningLimits("BaselineWarningLimits", 
				  dxActParam_.baselineWarningLimits);

   sqlstmt << "baselineWarningLimitsId = "
	   << recordBaselineWarningLimits.record(db)
	   << " " << conjunction << " ";

   RecordBaselineLimits 
      recordBaselineErrorLimits("BaselineErrorLimits", 
				dxActParam_.baselineErrorLimits);

   sqlstmt << "baselineErrorLimitsId = "
	   << recordBaselineErrorLimits.record(db);

   return sqlstmt.str();
}


RecordDxIntrinsics::RecordDxIntrinsics(
   const string & tableName,
   const struct DxIntrinsics& dxIntrinsics):
   RecordInDatabase(tableName),
   dxIntrinsics_(dxIntrinsics)
{
}


string RecordDxIntrinsics::stmt(MYSQL* db, const string & conjunction)
{
   stringstream sqlstmt;
 
   sqlstmt << "interfaceVersionNumber = '" 
	   << dxIntrinsics_.interfaceVersionNumber << "'"
	   << " " << conjunction << " "

	   << "hzPerSubchannel = "  << dxIntrinsics_.hzPerSubchannel
	   << " " << conjunction << " "

	   << "maxSubchannels = " << dxIntrinsics_.maxSubchannels
	   << " " << conjunction << " "

	   << "serialNumber = " << dxIntrinsics_.serialNumber
	   << " " << conjunction << " "

	   << "dxHostName = '" << dxIntrinsics_.name << "'"
	   << " " << conjunction << " "

	   << "dxCodeVersion = '" << dxIntrinsics_.codeVersion << "'"
	   << " " << conjunction << " "

	   << "birdieMaskDate = '" 
	   << SseUtil::isoDateTimeWithoutTimezone(
	      dxIntrinsics_.birdieMaskDate.tv_sec)
	   << "'"
	   << " " << conjunction << " "

	   << "permMaskDate = '" 
	   <<  SseUtil::isoDateTimeWithoutTimezone(
	      dxIntrinsics_.permMaskDate.tv_sec)
	   << "'";

   return sqlstmt.str();
}




RecordBaselineLimits::RecordBaselineLimits(
   const string & tableName,
   const BaselineLimits& baselineLimits):
   RecordInDatabase(tableName),
   baselineLimits_(baselineLimits)
{
}


string RecordBaselineLimits::stmt(MYSQL* db, const string & conjunction)
{
   stringstream sqlstmt;

   sqlstmt << "meanUpperBound = " << baselineLimits_.meanUpperBound
	   << " " << conjunction << " "

	   << "meanLowerBound = " << baselineLimits_.meanLowerBound
	   << " " << conjunction << " "

	   << "stdDevPercent = " << baselineLimits_.stdDevPercent
	   << " " << conjunction << " "

	   << "maxRange = " << baselineLimits_.maxRange;

   return sqlstmt.str();
}
