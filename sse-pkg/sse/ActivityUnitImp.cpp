/*******************************************************************************

 File:    ActivityUnitImp.cpp
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

// An ActivityUnit controls interactions with a dx on behalf
// of an activity: sending configuration information, storing
// and forwarding signal reports, etc.

#include <ace/OS.h>
#include "ActivityUnitImp.h"

#include "ActParameters.h"
#include "Assert.h"
#include "CondensedSignalReport.h"
#include "DbParameters.h"
#include "DbQuery.h"
#include "DebugLog.h"
#include "ExpandedSignalReport.h"
#include "MsgSender.h"
#include "MysqlQuery.h"
#include "ObserveActivity.h"
#include "OffPositions.h"
#include "DxParameters.h"
#include "DxProxy.h"
#include "RecentRfiMask.h"
#include "RecordInDatabase.h"
#include "RecordDxInfoInDb.h"
#include "ScienceDataArchive.h"
#include "SseArchive.h"
#include "SseAstro.h"
#include "SseMessage.h"
#include "SseMsg.h"
#include "SseDxMsg.h"
#include "SseUtil.h"
#include "sseVersion.h"
#include "StreamMutex.h"
#include "TestSigParameters.h"
#include <map>
#include <memory>

#include <cmath> // Needed for Linux

using namespace std;

// names of database tables used to store signals
static const string CandidateSignalTableName("CandidateSignals");
static const string SetiLiveCandidateSignalTableName("ZxCandidateSignals");
static const string CandidatePulseTrainTableName("CandidatePulseTrains");
static const string SignalTableName("Signals");
static const string PulseTrainTableName("PulseTrains");

static const int PrintPrecision = 9;  // MilliHz
static const double RecentRfiElementMinBandwidthMhz = 0.001;

static const string CwPowerSigType("CwP");
static const string CwCohSigType("CwC");
static const string PulseSigType("Pul");

// Mutex protected access to the act unit summary stream
#define ActUnitSummaryStrm() \
   StreamMutex(actUnitSummaryTxtStrm_, actUnitSummaryTxtStrmMutex_)

ActivityUnitImp::PulseTrain::PulseTrain()
   :
   pulseArray(0)
{
}

ActivityUnitImp::PulseTrain::~PulseTrain()
{
   delete[] pulseArray;
}

ActivityUnitImp::CandSigInfo::CandSigInfo()
   : dbTableKeyId(0)
{
}

struct CandSig
{
   SignalDescription descrip;
   ConfirmationStats cfm;
};

class LookUpCandidatesFromPrevAct: public DbQuery
{
public:
   LookUpCandidatesFromPrevAct(ActivityUnitImp *activityUnit,
                               MYSQL *callerDbConn,
                               int previousActId,
                               FollowUpSignalInfoList &infoList);
   virtual ~LookUpCandidatesFromPrevAct();

protected:
   string prepareQuery();
   void processQueryResults();

private:
   void processCandidates(FollowUpSignalInfoList &infoList,
			  int & duplicateCount);

   enum colIndices_ { activityIdCol, rfFreqCol, sigClassCol, reasonCol,
                      driftCol, resCol, actStartTimeSecsCol, dxNumberCol,
                      signalIdNumberCol, typeCol, pfaCol, snrCol, nRequestedCols};

   ActivityUnitImp *actUnit_;
   int previousActId_;
   FollowUpSignalInfoList &infoList_;
};

class LookUpCandidatesFromSetiLive: public DbQuery
{
public:
   LookUpCandidatesFromSetiLive(ActivityUnitImp *activityUnit,
                               MYSQL *callerDbConn,
		       ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
	             ActivityUnitImp::PulseTrainList &pulseTrainList,
	           ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList);

   virtual ~LookUpCandidatesFromSetiLive();

protected:
   string prepareQuery();
   void processQueryResults();

private:
   void processCandidates( ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
		      ActivityUnitImp::PulseTrainList &pulseTrainList,
		         ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList,
			  int & duplicateCount);

   void extractSignalDescription(MYSQL_ROW row, SignalDescription & descrip);
   void extractSignalPath(MYSQL_ROW row, SignalPath & path);
   void extractContainsBadBands(MYSQL_ROW row, bool_t & containsBadBands);
   void extractSignalId(MYSQL_ROW row, SignalId & signalId);
   void extractConfirmationStats(MYSQL_ROW row, ConfirmationStats & cfmStats);
   void extractPulseTrainDescription(MYSQL_ROW row, PulseTrainDescription & descrip);
   void getPulsesForSignal(MYSQL *conn, const string & pulseTrainTableName, 
		   unsigned int signalTableId, Pulse pulseArray[], 
		   int numberOfPulses);
   enum colIndices_
   {
      signalTableIdCol, activityIdCol, typeCol, 
      rfFreqCol, driftCol, widthCol, powerCol,
      polCol, sigClassCol, reasonCol, subchannelNumberCol, containsBadBandsCol,
      dxNumberCol, actStartTimeSecsCol, signalIdNumberCol,
      origDxNumberCol,  origActivityIdCol, origActStartTimeSecsCol,
      origSignalIdNumberCol,
      pfaCol, snrCol, nSegmentsCol,
      pulsePeriodCol, numberOfPulsesCol, resCol,
      nRequestedCols
   };

   ActivityUnitImp *actUnit_;
   int actId_;
   MYSQL *conn_;
   ActivityUnitImp::CwPowerSignalList &cwPowerSigList_;
   ActivityUnitImp::PulseTrainList &pulseTrainList_;
   ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList_;

};

class LookUpCandidatesFromCounterpartDxs: public DbQuery
{
public:
   LookUpCandidatesFromCounterpartDxs(
      ActivityUnitImp *activityUnit,
      MYSQL *callerDbConn,
      ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
      ActivityUnitImp::PulseTrainList &pulseTrainList,
      ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList);
   
   virtual ~LookUpCandidatesFromCounterpartDxs();

protected:
   string prepareQuery();
   void processQueryResults();

private:
   void processCandidates(ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
		      ActivityUnitImp::PulseTrainList &pulseTrainList,
		      ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList);

   void extractSignalDescription(MYSQL_ROW row, SignalDescription & descrip);
   void extractSignalPath(MYSQL_ROW row, SignalPath & path);
   void extractContainsBadBands(MYSQL_ROW row, bool_t & containsBadBands);
   void extractSignalId(MYSQL_ROW row, SignalId & signalId);
   void extractConfirmationStats(MYSQL_ROW row, ConfirmationStats & cfmStats);
   void extractPulseTrainDescription(MYSQL_ROW row, PulseTrainDescription & descrip);

   void getPulsesForSignal(MYSQL *conn,
                           const string & pulseTrainTableName, 
                           unsigned int signalTableId, 
                           Pulse pulseArray[], int numberOfPulses);

   enum colIndices_
   {
      signalTableIdCol, activityIdCol, typeCol, 
      rfFreqCol, driftCol, widthCol, powerCol,
      polCol, sigClassCol, reasonCol, subchannelNumberCol, containsBadBandsCol,
      dxNumberCol, actStartTimeSecsCol, signalIdNumberCol,
      origDxNumberCol,  origActivityIdCol, origActStartTimeSecsCol,
      origSignalIdNumberCol,
      pfaCol, snrCol, nSegmentsCol,
      pulsePeriodCol, numberOfPulsesCol, resCol,
      nRequestedCols
   };

   MYSQL *conn_;
   ActivityUnitImp *actUnit_;
   ActivityUnitImp::CwPowerSignalList &cwPowerSigList_;
   ActivityUnitImp::PulseTrainList &pulseTrainList_;
   ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList_;

};

class ResolveCandidatesBasedOnSecondaryProcessingResults: public DbQuery
{
public:
   ResolveCandidatesBasedOnSecondaryProcessingResults(ActivityUnitImp *activityUnit,
                                                      MYSQL *dbConn);
   virtual ~ResolveCandidatesBasedOnSecondaryProcessingResults();

protected:
   string prepareQuery();
   void processQueryResults();

private:
   typedef int SignalNumber;
   typedef multimap<SignalNumber, CandSig> CandSigMultiMap;

   void fetchCounterpartCandidates(CandSigMultiMap & candSigMultiMap);
   void extractCandSig(MYSQL_ROW row, CandSig & candSig);

   bool findCounterpartResultsForSignal(
      int signalNumber,
      double signalSnr,
      CandSigMultiMap & candSigMultiMap,
      bool & signalWasSeen);

   void updateCandidateSignalClassAndReasonInDb(
      const DbTableKeyId dbTableKeyId, 
      const SignalClass sigClass, const SignalClassReason reason);

   void countSignalAsConfirmed(const string &sigType, int signalNumber);

   enum colIndices_
   {
      dxNumberCol, typeCol, rfFreqCol, driftCol, sigClassCol,
      reasonCol, signalIdNumberCol, origSignalIdNumberCol, 
      pfaCol, snrCol, nRequestedCols
   };

   MYSQL *conn_;
   ActivityUnitImp *actUnit_;
   bool usingNulls_;
   double nullDepthLinear_;
};


class PrepareFakeSecondaryCandidatesToForceArchiving
{
public:

   PrepareFakeSecondaryCandidatesToForceArchiving(
      ActivityUnitImp *activityUnit,
      ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
      ActivityUnitImp::PulseTrainList &pulseTrainList,
      ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList);
   
   virtual ~PrepareFakeSecondaryCandidatesToForceArchiving();

private:

};

#ifdef FIND_TEST_SIGNAL
static bool matchCwPowerTestSignal(CwPowerSignal &cwp,
				   const TestSigParameters &testSigParameters,
				   double ifcSkyFreqMHhz);
#endif

ActivityUnitImp::ActivityUnitImp (
   ObserveActivity *activity, 
   int actUnitId,
   DxProxy* dxProxy,
   const DxActivityParameters &dxActParam,
   int verboseLevel)
   :
   dxActParam_(dxActParam),
   obsActivity_(activity), 
   dxProxy_(dxProxy),
   verboseLevel_(verboseLevel),
   scienceDataArchive_(0),
   condensedAllSignalReport_(0),
   condensedCandidateReport_(0),
   expandedAllSignalReport_(0),
   expandedCandidateReport_(0),
   actUnitId_(actUnitId),
   dbActivityUnitId_(0),
   dxBandLowerFreqLimitMHz_(0), 
   dxBandUpperFreqLimitMHz_(0),
   actualDxTunedFreqMhz_(0),
   dbParam_(activity->getDbParameters()),
   dbConn_(0),
   useDb_(false),
   zxMode_(false),
   beamNumber_(-1),
   targetId_(-1),
   detachedSelfFromDxProxy_(false),
   wrappedUp_(false),
   usingOffActNull_(activity->getActParameters().getOffActNullType() != ActParameters::NULL_NONE),
   nullDepthLinear_(SseUtil::dbToLinearRatio(activity->getActParameters().getNullDepthDb()))
{
   VERBOSE2(verboseLevel_,
	    "ActUnitImp constructor" << endl;);

   useDb_ = dbParam_.useDb();
   if (useDb_)
   { 
      // this will try to open a database connection
      // if one is not already open
      dbConn_ = dbParam_.getDb();
   }
	// Am I a Zx ?
    if (dxProxy->getName().compare(0, 2, "zx") == 0 ) zxMode_ = true;
    
   beamNumber_ = obsActivity_->getBeamNumberForDxName(dxProxy->getName());
   siteName_ = obsActivity_->getSiteName();
   targetId_ = obsActivity_->getTargetIdForBeam(beamNumber_);

   scienceDataArchive_ = new ScienceDataArchive(
      getOutputFilePrefix(), 
      dxProxy_->getName());

   scienceDataArchive_->truncateOutputFiles();

   createSignalReportFileStream(sigReportTxtStrm_,
				getOutputFilePrefix(),
				dxProxy_->getName());

   string dxTuningInfo = getDxTuningInfo(
      dxActParam,
      dxProxy_->getIntrinsics().hzPerSubchannel,
      dxProxy_->getIntrinsics().maxSubchannels);

   condensedCandidateReport_ = new CondensedSignalReport(
      getObsAct()->getActivityName(), getObsAct()->getId(),
      dxProxy->getName(),
      "Candidates & Candidate Results (condensed format)",
      dxTuningInfo);

   expandedCandidateReport_ = new ExpandedSignalReport(
      getObsAct()->getActivityName(), getObsAct()->getId(),
      dxProxy->getName(),
      "Candidates & Candidate Results (expanded format)",
      dxTuningInfo);

   condensedAllSignalReport_ = new CondensedSignalReport(
      getObsAct()->getActivityName(), getObsAct()->getId(),
      dxProxy->getName(),
      "All CW Power & Pulse Signals (condensed format)",
      dxTuningInfo);

   expandedAllSignalReport_ = new ExpandedSignalReport(
      getObsAct()->getActivityName(), getObsAct()->getId(),
      dxProxy->getName(),
      "All CW Power & Pulse Signals (expanded format)",
      dxTuningInfo);

   createActUnitSummaryFileStream();

   writeActUnitSummaryHeader();

   dxProxy_->attachActivityUnit(this);

   ActUnitSummaryStrm() << dxTuningInfo;

}
 
ActivityUnitImp::~ActivityUnitImp()
{
   delete scienceDataArchive_;

   delete condensedAllSignalReport_;
   delete condensedCandidateReport_;

   delete expandedAllSignalReport_;
   delete expandedCandidateReport_;

}

// Release all non-memory-related resources.  Do this
// here rather than in the destructor so that the
// resources are released as soon as the act unit
// completes its work.  

void ActivityUnitImp::releaseResources()
{
   detachSelfFromDxProxy();

   // close the signal reports output stream
   sigReportTxtStrm_.close();

   // close the actunit summary file
   actUnitSummaryTxtStrm_.close();

}

void ActivityUnitImp::detachSelfFromDxProxy()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(detachFromDxProxyMutex_);

   if (!detachedSelfFromDxProxy_)
   {
      detachedSelfFromDxProxy_ = true;
      dxProxy_->detachActivityUnit(this);
   }
}

int ActivityUnitImp::getId()
{
   return actUnitId_;
}

int ActivityUnitImp::getActivityId()
{
   return getObsAct()->getId();
}

int ActivityUnitImp::getVerboseLevel()
{
   return verboseLevel_;
}

ScienceDataArchive *ActivityUnitImp::getScienceDataArchive()
{
   return scienceDataArchive_;
}

ostream & ActivityUnitImp::getSigReportTxtStrm()
{
   return sigReportTxtStrm_;
}

ObsSummaryStats & ActivityUnitImp::getObsSummaryStats()
{
   return obsSummaryStats_;
}

ObserveActivity *ActivityUnitImp::getObsAct()
{
   return obsActivity_;
}

unsigned int ActivityUnitImp::getDbActivityUnitId()
{
   return dbActivityUnitId_;
}

CondensedSignalReport * ActivityUnitImp::getCondensedAllSignalReport()
{
   Assert(condensedAllSignalReport_);
   return condensedAllSignalReport_;
}

CondensedSignalReport * ActivityUnitImp::getCondensedCandidateReport()
{
   Assert(condensedCandidateReport_);
   return condensedCandidateReport_;
}

ExpandedSignalReport * ActivityUnitImp::getExpandedAllSignalReport()
{
   Assert(expandedAllSignalReport_);
   return expandedAllSignalReport_;
}

ExpandedSignalReport * ActivityUnitImp::getExpandedCandidateReport()
{
   Assert(expandedCandidateReport_);
   return expandedCandidateReport_;
}

string ActivityUnitImp::getOutputFilePrefix()
{
   return getObsAct()->getArchiveFilenamePrefix() + 
      "." + dxProxy_->getName() + ".";
}

// search for text inside a message, in a case insensitive way.
static bool substringFoundInMessage(const string &messageText, const string &searchText)
{
   if (SseUtil::strToLower(messageText).find(SseUtil::strToLower(searchText))
       != std::string::npos)
   {
      return true;
   }

   return false;

}

void ActivityUnitImp::sendDxMessage(DxProxy *dx,
				     const NssMessage & nssMessage)
{
   VERBOSE2(verboseLevel_,"received nss message: " << nssMessage
	    << " from dx " << dx->getName() << endl);

   // Terminate on any error or fatal message for now.
   // TBD:  possibly add more handling based on message code.

   if (nssMessage.severity == SEVERITY_ERROR ||
       nssMessage.severity == SEVERITY_FATAL)
   {
      stringstream strm;
	
      strm << "From: " << dx->getName()
	   << " Code: " << nssMessage.code 
	   << " Text: " << nssMessage.description << endl;

      // If the dx sends any errors that are known to get it
      // into a nonworking state, then disconnect it.
      // TBD 
      //    move this error handling code elsewhere
      //    use an error code rather than parsing the message text.

      const string startError("start obs error");
      const string archivingBug("No corresponding msg:DxDiskIO.cpp");

      if (substringFoundInMessage(nssMessage.description, startError)
	  || substringFoundInMessage(nssMessage.description, archivingBug))
      {
	 strm << "\nNonrecoverable dx error received"
	      << ", sending shutdown message, resetting socket on: " 
	      << dx->getName() << endl;
	 dx->shutdown();
	 dx->resetSocket();
      }
      else if (substringFoundInMessage(nssMessage.description, 
				       "signal not a candidate"))
      {
	 // this is nonfatal archiving bug, just keep going
	 return;
      }

      SseException except(strm.str(), 
			  __FILE__, __LINE__, SSE_MSG_START_OBS_ERROR,
			  SEVERITY_ERROR);
      terminateActivityUnit(dbParam_, except); 
   }

}


// Look up the confirmed candidates in the database that
// were acquired in the previous activity (identified with the previousActId)
// and which fall within the band of this dx.

void ActivityUnitImp::prepareFollowUpCandidateSignals(MYSQL *callerDbConn, int previousActId)
{
   VERBOSE2(verboseLevel_, "ActivityUnitImp::prepareFollowUpCandidateSignals for "
	    << dxProxy_->getName() << endl;);
   
   auto_ptr<LookUpCandidatesFromPrevAct> 
      lookUp(new LookUpCandidatesFromPrevAct(
                this, callerDbConn, previousActId, followUpSignalInfoList_));

   lookUp->execute();

   // TBD what to do if the list is empty
}



// Use the original frequency, start time, and drift rate, 
// and the new start time,  to project the frequency ahead for the
// candidates, then send them to the dx as follow up signals.
// Returns the number of non-duplicate signals sent.

int ActivityUnitImp::sendFollowUpCandidateSignals(const NssDate &newStartTime)
{
   VERBOSE2(verboseLevel_, "ActivityUnitImp::sendFollowUpSignals to "
	    << dxProxy_->getName() << endl;);
    
   if (followUpSignalInfoList_.size() > 0)
   {
      forwardFollowUpSignalsToDx(followUpSignalInfoList_, newStartTime);
   }

   return followUpSignalInfoList_.size();

   // TBD what to do if the list is empty
}

FollowUpSignalInfo & ActivityUnitImp::lookUpFollowUpSignalById(int signalIdNumber)
{
   for (FollowUpSignalInfoList::iterator it = followUpSignalInfoList_.begin();
	it != followUpSignalInfoList_.end(); ++it)
   {
      FollowUpSignalInfo & sigInfo = *it;
      
      if (sigInfo.followUpSignal.origSignalId.number == signalIdNumber)
      {
         return sigInfo;
      }
   }

   stringstream strm;
   strm << getDxName() << " ActivityUnitImp::lookUpFollowUpSignalById"
        << " could not find signalIdNumber " << signalIdNumber << endl;

   throw SseException(strm.str());
}


// send the CW & pulse signals on the list to the dx.
// 
void ActivityUnitImp::forwardFollowUpSignalsToDx(
   FollowUpSignalInfoList &infoList, const NssDate &newStartTime)
{
   Count nSignals;
   nSignals.count = infoList.size();  // total number of cw & pulse followup signals
   int activityId = getActivityId();

   dxProxy_->beginSendingFollowUpSignals(activityId, nSignals);

   // print the follow up signal info in a condensed format for easy reading
   // in the system log

   stringstream sigSummary;

   sigSummary << "Act " << getActivityId() << ":\n"
	      << dxProxy_->getName() 
	      << " following up signals: " << endl;

   // Also figure out which candidate has the highest power signal
   // and send a compamp datarequest to the dx using the projected freq
   // for that signal.

   float highestSnr = -1.0;
      FollowUpSignalInfo & firstSig = *infoList.begin();
      double dataRequestRfFreqMhz = firstSig.followUpSignal.rfFreq;

   sigSummary.precision(PrintPrecision);    
   sigSummary.setf(std::ios::fixed);  // show all decimal places up to precision
 
   sigSummary << "   Orig Freq MHz  " << "  Drift Hz/s " << "  Projected Freq MHz" << endl;
   sigSummary << "   ---------------" << "  -----------" << "  ------------------" << endl;

   int count = 0;

   for (FollowUpSignalInfoList::iterator it = infoList.begin();
	it != infoList.end();
	++it)
   {
      // pull the signals off the list
      FollowUpSignalInfo & sigInfo = *it;

      ActUnitSummaryStrm()
	 << "Original " << sigInfo.getSignalTypeString()
	 << " candidate (not projected): \n" << sigInfo.followUpSignal << endl;

      // orig freq, drift
      sigSummary << ++count << ") " <<  sigInfo.followUpSignal.rfFreq << "  " 
		 <<  sigInfo.followUpSignal.drift;

      // project signal ahead
      // tbd error handling if signal falls outside of dx band
      projectSignalAheadInFreq(sigInfo.followUpSignal, newStartTime);

      ActUnitSummaryStrm() 
	 << "Projected " << sigInfo.getSignalTypeString()
	 << " candidate: \n" << sigInfo.followUpSignal << endl << endl;

      // projected freq
      sigSummary << "  " << sigInfo.followUpSignal.rfFreq << endl;

      if (sigInfo.signalType == FollowUpSignalInfo::SigType_Pulse)
      {
	 FollowUpPulseSignal followUpPulseSignal; 
	 followUpPulseSignal.sig = sigInfo.followUpSignal;
	 dxProxy_->sendFollowUpPulseSignal(activityId, followUpPulseSignal);
      }
      else // Cw, power or coherent
      {
	 FollowUpCwSignal followUpCwSignal;
	 followUpCwSignal.sig = sigInfo.followUpSignal;
 
	 dxProxy_->sendFollowUpCwSignal(activityId, followUpCwSignal);
      }
      // TBD add check for invalid signalType

      if (sigInfo.cfm.snr > highestSnr)
      {
         highestSnr = sigInfo.cfm.snr;
         dataRequestRfFreqMhz = sigInfo.followUpSignal.rfFreq;
      }

   }

   dxProxy_->doneSendingFollowUpSignals(activityId);

   // send science data request for the desired candidate's rf freq
	   int subchan = getSubchannel( dataRequestRfFreqMhz);
	   scienceDataArchive_->setCurrentDataRequestSubchannel(subchan);
   sigSummary 
      << "---> Data request: " << dxProxy_->getName() << "  "
      <<  dataRequestRfFreqMhz 
      <<  "  MHz  <----- Subchan " << subchan << endl;

   sendScienceDataRequestFreq(dataRequestRfFreqMhz);

   SseArchive::SystemLog() << sigSummary.str() << endl;
}


void ActivityUnitImp::sendScienceDataRequestFreq(double rfFreqMhz)
{
   dxActParam_.scienceDataRequest.scienceData.requestType = REQ_FREQ;
   dxActParam_.scienceDataRequest.scienceData.rfFreq = rfFreqMhz;
   dxProxy_->dxScienceDataRequest(dxActParam_.scienceDataRequest);
}


// project the signal's frequency in MHz ahead from the time it was observed
// to what it should be at the new observation start time based
// on the signal drift (in Hz/sec)

void ActivityUnitImp::projectSignalAheadInFreq(FollowUpSignal &sigInfo,
					       const NssDate &newStartTime)
{
   int startDeltaTimeSecs = newStartTime.tv_sec - 
      sigInfo.origSignalId.activityStartTime.tv_sec;

   double freqShiftHz = startDeltaTimeSecs * sigInfo.drift;

   sigInfo.rfFreq = sigInfo.rfFreq + (freqShiftHz / SseAstro::HzPerMhz);

   VERBOSE2(verboseLevel_,
	    "projected candidate (delta time: " << startDeltaTimeSecs
	    << " secs, freqShiftHz: " << freqShiftHz << "\n" << sigInfo);

}


// For multitarget (multibeam) observations:
// Look up the confirmed candidates that counterpart dx
// have found in this dx's frequency band, and
// forward them for secondary processing.
// If "forced archiving" mode is enabled, then add extra 'fake'
// candidates to get additional archive data around the center tune
// frequency.

void ActivityUnitImp::sendCandidatesForSecondaryProcessing(MYSQL *callerDbConn)
{
   string methodName("ActivityUnitImp::sendCandidatesForSecondaryProcessing()");

   Assert(actOpsBitEnabled(MULTITARGET_OBSERVATION)
	  || actOpsBitEnabled(FORCE_ARCHIVING_AROUND_CENTER_TUNING));

   try
   {
      // Get lists of candidates
      CwPowerSignalList cwPowerSigList;
      CwCoherentSignalList cwCoherentSigList;
      PulseTrainList pulseTrainList;
     
      if (actOpsBitEnabled(MULTITARGET_OBSERVATION))
      {
         auto_ptr<LookUpCandidatesFromCounterpartDxs>
            lookUp(new LookUpCandidatesFromCounterpartDxs(
                      this, callerDbConn, cwPowerSigList,
                      pulseTrainList, cwCoherentSigList));
	 
	 lookUp->execute();
      }

      if (actOpsBitEnabled(FORCE_ARCHIVING_AROUND_CENTER_TUNING))
      {
	 // Use the secondary processing capability of the dx to do
	 // some extra archiving

	 PrepareFakeSecondaryCandidatesToForceArchiving fakeCandidates(
	    this, cwPowerSigList, pulseTrainList, cwCoherentSigList);
      }


      Count nCandidates;
      nCandidates.count = cwPowerSigList.size() + pulseTrainList.size();

      // Send cw power & pulse signals
      dxProxy_->beginSendingCandidatesSecondary(getActivityId(), nCandidates);

      CwPowerSignalList::iterator cwPowIt;
      for (cwPowIt = cwPowerSigList.begin(); cwPowIt != cwPowerSigList.end();
	   ++cwPowIt)
      {
	 dxProxy_->sendCandidateCwPowerSignalSecondary(getActivityId(), *cwPowIt);
      }

      PulseTrainList::iterator pulseTrainIt;
      for (pulseTrainIt = pulseTrainList.begin(); pulseTrainIt != pulseTrainList.end();
	   ++pulseTrainIt)
      {
	 PulseTrain *pulseTrain = *pulseTrainIt;
	 dxProxy_->sendCandidatePulseSignalSecondary(
	    getActivityId(), pulseTrain->hdr, pulseTrain->pulseArray);

	 delete pulseTrain;
      }
	
      dxProxy_->doneSendingCandidatesSecondary(getActivityId());


      // send coherent signals corresponding to cw power signals
      dxProxy_->beginSendingCwCoherentSignalsSecondary(getActivityId(), nCandidates);

      CwCoherentSignalList::iterator cwCohIt;
      for (cwCohIt = cwCoherentSigList.begin(); cwCohIt != cwCoherentSigList.end();
	   ++cwCohIt)
      {
	 dxProxy_->sendCwCoherentSignalSecondary(getActivityId(), *cwCohIt);
      }

      dxProxy_->doneSendingCwCoherentSignalsSecondary(getActivityId());

   }
   catch (SseException &except)
   {
      stringstream strm;
      strm << "caught exception in " << methodName << " "
	   <<  except.descrip();
      SseMessage::log(MsgSender, getActivityId(),
                      except.code(), except.severity(), strm.str(),
                      except.sourceFilename(), except.lineNumber());
   }	
   catch(...)
   {
      stringstream strm;
      strm << "caught unexpected exception in " << methodName << endl;
      SseMessage::log(MsgSender, getActivityId(),
                      SSE_MSG_EXCEPTION, SEVERITY_ERROR,
                      strm.str(), __FILE__, __LINE__);
   }
}




// Multibeam mode:
// For all the candidates on this dx, look for secondary processing
// results from counterpart dxs, to resolve the signal classification.
// If the signals are found in at least one counterpart dx, then
// they are classified as RFI/SEEN_MULTIPLE_BEAMS, else CAND/CONFIRM.
// Note: the signal's classification & reason is updated in place in
// the database.
// TBD error handling if there are no counterpart candidate results available.

void ActivityUnitImp::resolveCandidatesBasedOnSecondaryProcessingResults(MYSQL *callerDbConn)
{
   string methodName("ActivityUnitImp::resolveCandidatesBasedOnSecondaryProcessingResults()");

   VERBOSE2(verboseLevel_, methodName << " for "
	    << dxProxy_->getName() << endl;);

   if (zxMode_) getSetiLiveCandidates(callerDbConn);

   else if (actOpsBitEnabled(FORCE_ARCHIVING_AROUND_CENTER_TUNING)
       && !actOpsBitEnabled(MULTITARGET_OBSERVATION))
   {
      // The 'secondary candidates' are all fake ones.  No additional
      // processing is needed, so go straight to archiving

      sendArchiveRequestsForPrimaryCandidates();
      sendArchiveRequestsForSecondaryCandidates();
   }
   else 
   {
      auto_ptr<ResolveCandidatesBasedOnSecondaryProcessingResults> 
         resolve(new ResolveCandidatesBasedOnSecondaryProcessingResults(
                    this, callerDbConn));

      resolve->execute();
   }
}

void ActivityUnitImp::getSetiLiveCandidates(MYSQL *callerDbConn)
{
	string methodName("ActivityUnitImp::getSetiLiveCandidates()");

	VERBOSE2(verboseLevel_, methodName << " for "
	               << dxProxy_->getName() << endl;);

	      // Get lists of candidates
	             CwPowerSignalList cwPowerSigList;
	             CwCoherentSignalList cwCoherentSigList;
	             PulseTrainList pulseTrainList;
	      
	auto_ptr<LookUpCandidatesFromSetiLive>
	       	liveCandidates(new LookUpCandidatesFromSetiLive(
					this, callerDbConn, cwPowerSigList,
				        pulseTrainList, cwCoherentSigList));
	liveCandidates->execute();

	// Now send Signal Detection Complete and Activity Complete
   getObsAct()->signalDetectionComplete(this);
   activityUnitComplete();
}
void ActivityUnitImp::savePulseCandSigInfo(DbTableKeyId dbTableKeyId, 
					   const SignalDescription & descrip,
					   const ConfirmationStats &cfm)
{
   saveCandSigInfo(dbTableKeyId, PulseSigType, descrip, cfm);
}

void ActivityUnitImp::saveCwCohCandSigInfo(DbTableKeyId dbTableKeyId, 
					   const SignalDescription & descrip,
					   const ConfirmationStats & cfm)
{
   saveCandSigInfo(dbTableKeyId, CwCohSigType, descrip, cfm);
}

void ActivityUnitImp::saveCandSigInfo(DbTableKeyId dbTableKeyId,
				      const string &sigType,
				      const SignalDescription & descrip,
				      const ConfirmationStats & cfm)
{
   Assert(sigType == CwCohSigType || sigType == PulseSigType);

   CandSigInfo candSigInfo;

   candSigInfo.dbTableKeyId = dbTableKeyId;
   candSigInfo.sigType = sigType;
   candSigInfo.descrip = descrip;
   candSigInfo.cfm = cfm;

   candSigInfoList_.push_back(candSigInfo);
}

void ActivityUnitImp::saveSecondarySignalDescrip(const SignalDescription &descrip)
{
   secondarySignalDescripList_.push_back(descrip);
}

bool ActivityUnitImp::actOpsBitEnabled(const ObserveActivityOperations & opsBit)
{
   return getObsAct()->getOperations().test(opsBit);
}



/* 
   Create a recent RFI mask and forward it to the dx.
   Don't include signals from the current target id.
*/
void ActivityUnitImp::sendRecentRfiMask(MYSQL *callerDbConn,
                                        const vector<TargetId> & targetsToExclude)
{    
   const string methodName("sendRecentRfiMask: ");

   VERBOSE2(verboseLevel_,"create " << " recent rfi mask for "
	    << dxProxy_->getName() << endl;);
   try
   {
      /*
       Limit the band covered by the mask to the dx bandwidth. 
       Note: dx band center is slightly asymmetric; also the actual tuned
       sky freq can vary by a few KHz from the requested one, so widen
       the nominal mask bandwidth a bit to make sure the mask fully
       covers the dx lower & upper band limits (end overlaps should
       be safely ignored by the dx).
      */
      double maskWideningFactorMhz = 0.010;

      if (zxMode_) maskWideningFactorMhz = 0.0;
      double bandCoveredCenterFreqMhz = dxProxy_->getDxSkyFreq(); 
      double bandCoveredWidthMhz = dxProxy_->getBandwidthInMHz()
	 + maskWideningFactorMhz; 

      VERBOSE2(verboseLevel_,"centerFreq " << bandCoveredCenterFreqMhz
	       << " bandwidth " << bandCoveredWidthMhz << endl;);

      double beginFreqMhz = bandCoveredCenterFreqMhz - 
	 (bandCoveredWidthMhz/2.0);

      double endFreqMhz = bandCoveredCenterFreqMhz + 
	 (bandCoveredWidthMhz/2.0);

      vector<double> signalFreqMhz;
      getRecentRfiSignals(callerDbConn, beginFreqMhz, endFreqMhz, targetsToExclude,
			  signalFreqMhz);

      vector<double> maskCenterFreqMhz;
      vector<double> maskWidthMhz;

      double minMaskElementWidthMhz(
	 getObsAct()->getDxParameters().getRecentRfiMaskElementWidthMinHz() /
	 SseAstro::HzPerMhz);

      RecentRfiMask::createMask(signalFreqMhz, 
				minMaskElementWidthMhz, 
				maskCenterFreqMhz, maskWidthMhz);

      unsigned int maskSizeToUse = adjustRecentRfiMaskSize(
	 maskCenterFreqMhz.size());
      
      // create list of subchannels for compamp data request
      //
      if (zxMode_)
      {
	      time_t currentTime;
	      struct tm hhmmss;
	      time(&currentTime);
	      gmtime_r(&currentTime, &hhmmss );
	      int seed1 = hhmmss.tm_min;
	      int seed2 = hhmmss.tm_sec;

      vector<int> compampSubchannels;
      unsigned int maxCompampSubchannels(getObsAct()->getDxParameters().getDataRequestMaxCompampSubchannels());
      int maxDxSubchannels(dxProxy_->getIntrinsics().maxSubchannels);
      

      if ( maskSizeToUse == 0 )
      {
#ifdef obsolete
     	      maxCompampSubchannels = 1;
	 if (dxActParam_.scienceDataRequest.scienceData.requestType == REQ_SUBCHANNEL)
	 {
         compampSubchannels.push_back(
        		 dxActParam_.scienceDataRequest.scienceData.subchannel);
	 }
	  else
	  {
	    int subchan = getSubchannel(
	    		dxActParam_.scienceDataRequest.scienceData.rfFreq);

	      compampSubchannels.push_back(subchan);
	       }
#endif
// Empty Recent Rfi Mask, pick maxCompampSubchannels started at seed1 and 
// increasing in intervals of seed
//
	int subChanNum;

	  for (unsigned int i = 0; i < maxCompampSubchannels; i++)
	  {
		  subChanNum = 2*seed1 + 2*i*(seed2+1);
		  if (subChanNum >= maxDxSubchannels ) break;
		  compampSubchannels.push_back( subChanNum);
	  }
      }
      else
	      // The Rfi mask is not empty
      {
	      // Convert Recent Rfi mask to a list of subchannels
	      //
		vector<int> subchannelList;

	   double subchannelWidthMhz = 
		   dxProxy_->getIntrinsics().hzPerSubchannel/SseAstro::HzPerMhz;
	   double lowFreq = dxProxy_->getDxSkyFreq() - 
		   ( subchannelWidthMhz + dxProxy_->getBandwidthInMHz())/2.0;
	   double highFreq = dxProxy_->getDxSkyFreq() + 
		   (dxProxy_->getBandwidthInMHz() - subchannelWidthMhz)/2.0;


              for (unsigned int signalIndex=0; 
			      signalIndex < maskSizeToUse;
		               ++signalIndex)
	        {
			double centerFreq = maskCenterFreqMhz[signalIndex];
			double maskWidth = maskWidthMhz[signalIndex];
			int nSubchan = maskWidth/subchannelWidthMhz;
			if (nSubchan > 1 )
			{
				// use all the subchannels in the mask entry
			double subchannelFreq = centerFreq - .5*maskWidth + 
						subchannelWidthMhz/2 ;
			while (subchannelFreq < lowFreq ) 
				subchannelFreq += subchannelWidthMhz;
			// For each subchannel in a wide mask
			while (subchannelFreq < centerFreq + .5*maskWidth )
				{
				if (subchannelFreq > highFreq) break;
				int subchan = getSubchannel( subchannelFreq );

				subchannelList.push_back(subchan);

				subchannelFreq += subchannelWidthMhz;
				}
			}
			else
			{
			int subchan = getSubchannel(centerFreq);

			subchannelList.push_back(subchan);
			}
		}
           if ( maxCompampSubchannels >= subchannelList.size())
           {
              for (unsigned int signalIndex=0; 
			      compampSubchannels.size() < subchannelList.size();
		               ++signalIndex)
	        {
			compampSubchannels.push_back(subchannelList[signalIndex]);
		}
	      if ( compampSubchannels.size() < maxCompampSubchannels)
	      {
		      // Add more subchannels until at max.
		int lastSize = compampSubchannels.size();
		int lastSubChan = compampSubchannels[lastSize-1];

		for ( unsigned int i = 0; i < (maxCompampSubchannels-lastSize); i++)
		{
			int subChan = lastSubChan + seed1 + i*seed2;
			if (subChan >= maxDxSubchannels) break;
			compampSubchannels.push_back(subChan);
		}
	      }
           }
      else if ( maxCompampSubchannels < subchannelList.size())
      {
	      // More subchannels in mask than max Compamps Subchannels
	      // Select subchannels pseudo-randomly
	      int seed3 = subchannelList.size()/maxCompampSubchannels;
	      int seed4 = seed1 % seed3;
	      int seed5 = (seed2 % seed3) + 1;

	      for (unsigned int i = 0; i < maxCompampSubchannels; i++)
	      {
		      int subchan = subchannelList[seed4+i*seed5];
		      if (subchan >= maxDxSubchannels) break;
		      compampSubchannels.push_back(subchan);
	      }
      }
      }
       // Send compamp subchannels to the Science Data Archive
       scienceDataArchive_->prepareCompampSubchannelFiles(
		        getOutputFilePrefix(),
			      dxProxy_->getName(), targetId_,
		       compampSubchannels);
       // Send compamp subchannels to the ZX
       	sendRequestedCompampSubchannels(compampSubchannels);
      }
      if (maskSizeToUse == 0)
      {
	 VERBOSE2(verboseLevel_,"Recent RFI mask is empty for " 
		  << dxProxy_->getName() << endl;);

	 ActUnitSummaryStrm() << "Recent RFI mask is empty" 
                              << "\n" << endl;
	 return;
      }

      sendRecentRfiMaskToDx(bandCoveredCenterFreqMhz, 
			     bandCoveredWidthMhz,
			     maskCenterFreqMhz, maskWidthMhz, 
			     maskSizeToUse);

   }
   catch (SseException &except)
   {
      // Just log the error, since the system can run without the 
      // recent rfi mask

      stringstream strm;
      strm << methodName << except.descrip()
	   << "Failed to create mask for dx: " 
	   << dxProxy_->getName() << endl;

      SseMessage::log(
	 MsgSender, getActivityId(),
	 except.code(), except.severity(), strm.str(),
	 except.sourceFilename(), except.lineNumber());

      ActUnitSummaryStrm() << strm.str();
   }	
}

void ActivityUnitImp::sendRequestedCompampSubchannels(
		const vector<int> & compampSubchannels)
{
	// Send begin sending compampSubchannels
	// with size of list
	//
	
   Count nSubchannels;
   nSubchannels.count = compampSubchannels.size();  

	dxProxy_->beginSendingRequestedCompampSubchannels(getActivityId(),
	 		nSubchannels );

	// Send each compampSubchannel
	//
       for (unsigned int index=0; index < compampSubchannels.size();
		               ++index)
	        {
	        dxProxy_->sendRequestedCompampSubchannel(getActivityId(),
				compampSubchannels[index]);
		}

	// Send done sendings compampSubchannels
	//
	dxProxy_->doneSendingRequestedCompampSubchannels(getActivityId());

}
/*
  Dx has a max upper limit on the size of the recent rfi mask
  that it will accept.  If the current mask size is 
  too large, then reduce it to the size the dx can handle.
*/
int ActivityUnitImp::adjustRecentRfiMaskSize(
   int currentMaskSize)
{
   int maxDxRecentRfiMaskSize =
      getObsAct()->getDxParameters().getRecentRfiMaskSizeMax();
   if (maxDxRecentRfiMaskSize < 0)
   {
      maxDxRecentRfiMaskSize = 0;
   }
   
   int maskSizeToUse(currentMaskSize);
   if (maskSizeToUse > maxDxRecentRfiMaskSize)
   {
      stringstream strm;
      strm << "Recent RFI mask size of " 
	   << maskSizeToUse 
	   << " exceeds the max dx mask size of "
	   <<  maxDxRecentRfiMaskSize
	   << ".  Mask truncated to max size for dx: "
	   << dxProxy_->getName() << ".\n";
      
      VERBOSE2(verboseLevel_,strm.str());
      
      SseMessage::log(MsgSender, getActivityId(),
                      SSE_MSG_RFI_MASK_2LONG, SEVERITY_WARNING,
                      strm.str(), __FILE__, __LINE__);

      maskSizeToUse = maxDxRecentRfiMaskSize;
   }

   return maskSizeToUse;

}


void ActivityUnitImp::getRecentRfiSignals(
   MYSQL *callerDbConn,
   double beginFreqMhz,
   double endFreqMhz,
   const vector<TargetId> & targetIdsToExclude,
   vector<double> & signalFreqMhz)
{
   const string methodName("getRecentRfiSignals");

   stringstream sqlStmt;

   sqlStmt.precision(PrintPrecision);  // show N places after the decimal
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

   // get all the signals that are not on the targetsToExclude list,
   // are in this dx's freq band, and that are no older than the age limit 

   const double ageLimitDays = 
      getObsAct()->getActParameters().getRecentRfiAgeLimitDays();
   double ageLimit;
   double minCwPower;
   double minPulsePower;

   if (zxMode_) 
   {
   ageLimit = 365;
   minCwPower = 350.;
   minPulsePower = 500.;
   }
   else 
   {
   ageLimit = ageLimitDays;
   minCwPower = 0.0;
   minPulsePower = 0.0;
   }

   const int ageLimitSecs = static_cast<int>(ageLimit * SseAstro::SecsPerDay);

   sqlStmt << "SELECT distinct rfFreq from " << SignalTableName << " "
	   << "where"
	   << " rfFreq > " << beginFreqMhz
	   << " and rfFreq < " << endFreqMhz
	   << " and ((type = 'CwP' and power > " << minCwPower
	   << ") or ( type = 'Pul' and power > " << minPulsePower << " )) "
	   << " and UNIX_TIMESTAMP(activityStartTime) >="
	   << " (UNIX_TIMESTAMP() - " << ageLimitSecs << ")";

   stringstream sqlStmtPrefix;
   sqlStmtPrefix << sqlStmt.str() << " ... (" << targetIdsToExclude.size()
                 << " exclusion targetIds omitted)";

   if (! targetIdsToExclude.empty() && !zxMode_)
   {
      sqlStmt << " and targetId not in (";

      for (unsigned int i=0; i < targetIdsToExclude.size(); ++i)
      {
         sqlStmt << targetIdsToExclude[i] << ", ";
      }
      
      sqlStmt << "-1)";
   }
   
   sqlStmt << " order by rfFreq";
   
   enum colIndicies {rfFreqCol, numCols};

   VERBOSE2(verboseLevel_,
	    "Recent RFI mask query:\n" << sqlStmtPrefix.str() << endl;);

   ActUnitSummaryStrm() << "Recent RFI mask query: \n " 
                        << sqlStmtPrefix.str() << endl << endl; 

   MysqlQuery query(callerDbConn);
   query.execute(sqlStmt.str(), numCols, __FILE__, __LINE__);

   // fetch the signal freqs:
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      double rfFreqMhz(MysqlQuery::getDouble(row, rfFreqCol, 
					     __FILE__, __LINE__));

      signalFreqMhz.push_back(rfFreqMhz);
   }

}

/*
  Send Recent RFI mask.
  The freq and width of each mask element is contained
  in the maskCenterFreqMhz and maskWidthMhz vectors.
  The mask length is specified in the maskSizeToUse parameter,
  which may be smaller than the sizes of those two vectors.
 */

void ActivityUnitImp::sendRecentRfiMaskToDx(
   double bandCoveredCenterFreqMhz, 
   double bandCoveredWidthMhz,
   const vector<double> & maskCenterFreqMhz, 
   const vector<double> & maskWidthMhz, 
   unsigned int maskSizeToUse)
{
   RecentRfiMaskHeader header;
   header.numberOfFreqBands = maskSizeToUse;
   header.bandCovered.centerFreq = bandCoveredCenterFreqMhz;
   header.bandCovered.bandwidth = bandCoveredWidthMhz;
   header.excludedTargetId = -1;  // use any value, dx does not care

   VERBOSE2(verboseLevel_,"Recent RFI mask has " << header.numberOfFreqBands 
	    << " freq bands for " << dxProxy_->getName() << endl;);
   
   ActUnitSummaryStrm() << "Recent RFI mask has " 
                        << header.numberOfFreqBands 
                        << " freq bands.\n" << endl;
   
   Assert(static_cast<unsigned int>(header.numberOfFreqBands) <= 
	  maskCenterFreqMhz.size());
   
   Assert(maskCenterFreqMhz.size() == maskWidthMhz.size());
   
   vector<FrequencyBand> freqBandArray(header.numberOfFreqBands);
   for (int i=0; i < header.numberOfFreqBands; ++i)
   {
      freqBandArray[i].centerFreq = maskCenterFreqMhz[i]; 
      freqBandArray[i].bandwidth = maskWidthMhz[i];
   }
   
   // Send the mask on to the dx
   dxProxy_->sendRecentRfiMask(header, &freqBandArray[0]);
   
   // save the mask to a file in the sonata_archive
   saveRecentRfiMaskToArchiveFile(header, &freqBandArray[0]);

}

void ActivityUnitImp::saveRecentRfiMaskToArchiveFile(
   const RecentRfiMaskHeader &header, 
   FrequencyBand freqBandArray[])
{
   string filename;

   filename += getObsAct()->getArchiveFilenamePrefix();
   filename += ".";
   filename += dxProxy_->getName();
   filename += ".recent_rfi_mask.txt";

   ofstream strm;
   SseUtil::openOutputFileStream(strm, filename);

   VERBOSE2(verboseLevel_,"opening file: " << filename << endl); 

   strm << "Recent RFI mask for dx: " << dxProxy_->getName() << "\n\n";

   SseDxMsg::printRecentRfiMask(strm, header, freqBandArray);

   strm << endl;

   strm.close();
}


void ActivityUnitImp::terminateActivityUnit(DbParameters &callerDbParam,
                                            SseException &except)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(wrappedUpMutex_);

   if (! wrappedUp_)
   {
      wrappedUp_ = true;

      VERBOSE2(verboseLevel_, "ActUnitImp::terminateActivityUnit() ActUnitId: "
	       << getId() << endl;);

      sendStopMsgToDx();

      stringstream errorTextStrm;
      errorTextStrm << " Activity Unit " << getId()
		    << " (" << getDxName() << ")"
		    << " *Failed* " << except.descrip();
      
      // Only log error if it's not due to a stop request
      // so that the logs don't get cluttered
      if (!stopCommandReceived_.get())
      {
	 SseMessage::log(getDxName(),
                         getActivityId(), SSE_MSG_ACT_UNIT_FAILED,
                         SEVERITY_ERROR, errorTextStrm.str(),
                         except.sourceFilename(), except.lineNumber());
      }

      if (callerDbParam.useDb())
      {
	 updateDbErrorComment(callerDbParam.getDb(), errorTextStrm.str());
      }

      ActUnitSummaryStrm() 
         << "\n"
         << errorTextStrm.str() << endl
         << SseUtil::currentIsoDateTime() << endl;

      // try to save any signals we have so far
      saveSignalReports();
      
      releaseResources();
      
      // Warning: ActivityUnit may be destroyed in this method.
      // Do not use ActivityUnit past this point.
      
      getObsAct()->activityUnitFailed(this);

   }
}

// ----------- utility methods ---------------

string ActivityUnitImp::getDxTuningInfo(const DxActivityParameters &dxActParam,
					 double hzPerDxSubchannel,
                                         int numberOfSubchannels)
{
   // Log the dx tune center freq, dx bandwidth,
   // and dx band lower & upper edge freqs.
   // Dx center freq is in the middle of the subchannel that's
   // one up from the halfway point (eg, subchannel 1025 in a 2048 subchannel
   // bandwidth), so the lower half is 1/2 subchannel wider than the upper.
   // (Assumes an even number of subchannels).

   const double MhzPerDxSubchannel = hzPerDxSubchannel / SseAstro::HzPerMhz;

   double nLowerSubchannels = (numberOfSubchannels / 2) + 0.5;
   double nUpperSubchannels = (numberOfSubchannels / 2) - 0.5;

   dxBandLowerFreqLimitMHz_ = dxActParam.dxSkyFreq -
      (nLowerSubchannels * MhzPerDxSubchannel);
   dxBandUpperFreqLimitMHz_ = dxActParam.dxSkyFreq +
      (nUpperSubchannels * MhzPerDxSubchannel);

   double dxBandwidthInMHz = numberOfSubchannels * MhzPerDxSubchannel;

   stringstream strm;
   strm.precision(PrintPrecision); 
   strm.setf(std::ios::fixed);      // show all decimal places up to precision


   strm 
      << "# Dx SkyFreq: " 
      << dxActParam.dxSkyFreq << " MHz" << endl

      << "# Ifc SkyFreq: " 
      << dxActParam.ifcSkyFreq << " MHz" << endl

      << "# Rcvr (ATA RF tuning) SkyFreq: " 
      << dxActParam.rcvrSkyFreq << " MHz" << endl

      << "# Dx Assigned Bandwidth: " 
      << dxBandwidthInMHz  << " MHz "
      << "(" << numberOfSubchannels << " subchannels) " << endl

      << "# Dx Band Edges: " << dxBandLowerFreqLimitMHz_ << " MHz -> "
      << dxBandUpperFreqLimitMHz_ << " MHz" << endl
      << endl;

   return strm.str();
}

void ActivityUnitImp::createSignalReportFileStream(ofstream &strm,
						   const string &outputFilePrefix,
						   const string & dxId)
{
   string filename(outputFilePrefix);
   filename += "sigreports.txt";

   SseUtil::openOutputFileStream(strm, filename);

   VERBOSE2(verboseLevel_,"opening file: " << filename << endl); 

   strm.precision(PrintPrecision);           // show N places after the decimal
   strm.setf(std::ios::fixed);  // show all decimal places up to preci

   strm << "==================" << endl;
   strm << "NSS Signal Reports" << endl;
   strm << "==================" << endl;
   strm << "Activity Name: " << getObsAct()->getActivityName() << endl;
   strm << "Activity Id:   " << getObsAct()->getId() << endl;
   strm << "Creation Time: " << SseUtil::currentIsoDateTime() << endl;
   strm << "Dx Id:        " << dxId << endl;
   strm << endl;
   strm << endl;
}


void ActivityUnitImp::createActUnitSummaryFileStream()
{
   string summaryFilename(getOutputFilePrefix() + "summary.txt");
    
   SseUtil::openOutputFileStream(actUnitSummaryTxtStrm_, summaryFilename);

   // show N places after the decimal
   actUnitSummaryTxtStrm_.precision(PrintPrecision); 

   // show all decimal places up to precision
   actUnitSummaryTxtStrm_.setf(std::ios::fixed); 
			 
   VERBOSE2(verboseLevel_,"opening file: " <<  summaryFilename << endl); 
}

void ActivityUnitImp::writeActUnitSummaryHeader()
{
   ActUnitSummaryStrm()
   << "=====================" << endl
   << "Activity Unit Summary" << endl
   << "=====================" << endl
   << endl
   
   << "Activity Name: " << getObsAct()->getActivityName() << endl
   << "Activity Id: " << getObsAct()->getId() << endl
   << "Activity Unit Id: " << getId() << endl
   << "Activity Unit Creation Time: "
   << SseUtil::currentIsoDateTime() << endl
   << "Target Id: " << targetId_ << endl
   << SSE_VERSION << endl
   << endl    
   
   << "Dx: " << dxProxy_->getName() << endl
   << "Dx Version: " 
   << dxProxy_->getIntrinsics().codeVersion 
   << endl << endl;
   
}

void ActivityUnitImp::saveSignalReports()
{
   // save the condensed candidates
   condensedCandidateReport_->saveToFile(getOutputFilePrefix()
					 + "condensed.candidates.txt");

   // save the expanded candidates
   expandedCandidateReport_->saveToFile(getOutputFilePrefix()
					+ "expanded.candidates.txt");

   // Save the condensed "all signal" report.
   condensedAllSignalReport_->saveToFile(getOutputFilePrefix()
					 + "condensed.sigreports.txt");

   // Save the expanded "all signal" report.
   expandedAllSignalReport_->saveToFile(getOutputFilePrefix()
					+ "expanded.sigreports.txt");

}

void ActivityUnitImp::sendCandidateReportsToSystemLog()
{
   // Send the condensed candidate report to the system log.
   // Need to put it into a string stream first to
   // get around a copy constructor problem with the SystemLog.

   stringstream strm;
   strm << endl << *condensedCandidateReport_; 
   SseArchive::SystemLog() << strm.str();

}

void ActivityUnitImp::activityUnitComplete()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(wrappedUpMutex_);

   if (! wrappedUp_)
   {
      wrappedUp_ = true;

      VERBOSE2(verboseLevel_,
	       "ActUnit::activityUnitComplete()" << endl;);
      
      ActUnitSummaryStrm()
         << "\n"
         << obsSummaryStats_ << endl
         << "Activity Unit Complete: "
         << SseUtil::currentIsoDateTime() << endl;
      
      saveSignalReports();
      
      sendCandidateReportsToSystemLog();
      
      getObsAct()->activityUnitObsSummary(this, obsSummaryStats_);
      
      if (useDb_) 
      {
	 updateStats();
      }
      
      releaseResources();
      
      // -----------------------------------------------------
      // Warning: The ActivityUnit may be destroyed after reporting
      // in as complete to the activity.  Do not add any code after this comment
      // or in any way make use of the ActivityUnit after the
      // getObsAct()->activityUnitComplete call is made.
      
      getObsAct()->activityUnitComplete(this);
   }
}

// Send archive/discard requests based on the signal description
// and UI parameters.
// Note: It's not valid to send archive/discard requests to the
// dx for signals it did not find (ie, in followups).

void ActivityUnitImp::determineArchiveRequest(
   DxProxy *dx,
   const SignalDescription &descrip)
{
   const int SIGNAL_NOT_FOUND_ID_NUMBER = -1;

   if (descrip.signalId.number != SIGNAL_NOT_FOUND_ID_NUMBER)
   {
      ArchiveRequest archiveRequest;
      archiveRequest.signalId = descrip.signalId;

      ActParameters::CandidateArchiveOption archiveOpt = 
         getObsAct()->getActParameters().getCandidateArchiveOption();
    
      if (archiveOpt == ActParameters::ARCHIVE_ALL_CANDIDATES ||
          (archiveOpt == ActParameters::ARCHIVE_CONFIRMED_CANDIDATES && 
           (descrip.reason == CONFIRM || descrip.reason == RECONFIRM)))
      {
         dx->requestArchiveData(getActivityId(), archiveRequest);
      }
      else
      {
         dx->discardArchiveData(getActivityId(), archiveRequest);
      }
   }


}



// -----------------------------
// ----- database commands -----
// -----------------------------


static string BoolToQuotedYesNo(bool value)
{
   if (value)
   {
      return "'Yes'";
   }
   else
   {
      return "'No'";
   }
}


// If the PFA value is NaN, return
// a predefined "minimum pfa" value instead that
// will be readily identifiable and that mysql
// will accept.
static double ValidPfa(double pfa)
{
   const double minPfa = -9.99e99;

   if (isnan(pfa))
   {
      return minPfa;
   }

   return pfa;
}


void ActivityUnitImp::updateStats()
{
   const string methodName("updateStats");

   try
   {
      if (!getDbActivityUnitId())
      {
	 throw SseException("::updateStats() Uninitialized DB Activity ID\n",
			    __FILE__, __LINE__,
			    SSE_MSG_DBERR, SEVERITY_WARNING );
      }

      stringstream sqlStmt;

      sqlStmt << "UPDATE ActivityUnits SET "
	      << getObsAct()->prepareObsSummStatsSqlUpdateStmt(obsSummaryStats_)
	      << ", validObservation = 'Yes'"
	      << " where id = " << getDbActivityUnitId()
	      << " ";

      submitDbQueryWithThrowOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

   }
   catch (SseException &except)
   {
      SseMessage::log(MsgSender,
                      getActivityId(), except.code(),
                      except.severity(), 
                      except.descrip(),
                      except.sourceFilename(), except.lineNumber());
   }

}



void ActivityUnitImp::updateDbErrorComment(MYSQL *callerDbConn, const string& comment)
{
   const string methodName("updateDbErrorComment");

   try
   {
      if (!getDbActivityUnitId()) 
      {
	 throw SseException( "::updateDbErrorComment() "
			     "Uninitialized DB Activity ID\n",
			     __FILE__, __LINE__,
			     SSE_MSG_DBERR, SEVERITY_WARNING );
      }

      stringstream sqlStmt;
      sqlStmt << "UPDATE ActivityUnits SET "
	      << "comments = '"
	      << SseUtil::insertSlashBeforeSubString(comment, "'")
	      << "' where id = "
	      << getDbActivityUnitId()
	      << " ";

      submitDbQueryWithThrowOnError(callerDbConn, sqlStmt.str(), methodName, __LINE__);

   }
   catch (SseException &except)
   {
      SseMessage::log(MsgSender,
                      getActivityId(), except.code(),
                      except.severity(), 
                      except.descrip(),
                      except.sourceFilename(), except.lineNumber());

   }


}


// convert the fields in a signal Id into 
// a stream format suitable for updating the database.

void ActivityUnitImp::putSignalIdIntoSqlStatement(
   ostream &sqlStmt,
   const SignalId &signalId)
{
   // ----- signal id -----

   sqlStmt << ", dxNumber = "
	   << signalId.dxNumber;

  // store the start time in a datetime field
  // (set here as an ISO date-time string)

   sqlStmt << ", activityStartTime = '" 
	   << SseUtil::isoDateTimeWithoutTimezone(
	      signalId.activityStartTime.tv_sec)
	   << "'";

   sqlStmt << ", signalIdNumber = " 
	   << signalId.number;

}

// convert the fields in an original signal Id into 
// a stream format suitable for updating the database.

void ActivityUnitImp::putOrigSignalIdIntoSqlStatement(
   ostream &sqlStmt, const SignalId &origSignalId)
{
   // ----- orig signal id -----

   sqlStmt << ", origDxNumber = "
	   << origSignalId.dxNumber;

   sqlStmt << ", origActivityId = " 
	   << origSignalId.activityId;

  // store the start time in a datetime field
  // (set here as an ISO date-time string)

   sqlStmt << ", origActivityStartTime = '" 
	   << SseUtil::isoDateTimeWithoutTimezone(
	      origSignalId.activityStartTime.tv_sec)
	   << "'";

   sqlStmt << ", origSignalIdNumber = " 
	   << origSignalId.number;

}

// convert the fields in a SignalDescription into 
// a stream format suitable for updating the database.

void ActivityUnitImp::putSignalDescriptionIntoSqlStatement(
   ostream &sqlStmt, const SignalDescription &sig)
{
   sqlStmt.precision(PrintPrecision);
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

   sqlStmt << ", rfFreq = " << sig.path.rfFreq;

   sqlStmt.precision(PrintPrecision);

   sqlStmt << ", drift = "
	   << sig.path.drift;

   sqlStmt << ", width = "
	   << sig.path.width;

   sqlStmt << ", power = "
	   << sig.path.power;

   sqlStmt << ", pol = '"
	   << SseMsg::polarizationToString(sig.pol)
	   << "'";

   sqlStmt << ", sigClass = '"
	   << SseDxMsg::signalClassToString(sig.sigClass)
	   << "'";

   sqlStmt << ", reason = '" 
	   << SseDxMsg::signalClassReasonToBriefString(sig.reason)
	   << "'";

   sqlStmt << ", subchanNumber = " << sig.subchannelNumber;

   sqlStmt << ", containsBadBands = " 
	   << BoolToQuotedYesNo(sig.containsBadBands);

   putSignalIdIntoSqlStatement(sqlStmt, sig.signalId); 

   putOrigSignalIdIntoSqlStatement(sqlStmt, sig.origSignalId);


}

DbTableKeyId ActivityUnitImp::recordCandidate(
   const PulseSignalHeader& pulseSignalHdr,
   Pulse pulses[], const string& location) 
{
   return record(CandidateSignalTableName, CandidatePulseTrainTableName, 
		 pulseSignalHdr, pulses, location);
}

void ActivityUnitImp::recordSignal(const PulseSignalHeader& pulseSignalHdr,
				 Pulse pulses[], const string& location) 
{
   updateObsSummarySignalCounts(pulseSignalHdr.sig.reason);

   record(SignalTableName, PulseTrainTableName, 
	  pulseSignalHdr, pulses, location);
}

DbTableKeyId ActivityUnitImp::record(
   const string &signalTableName,
   const string &pulseTrainTableName,
   const PulseSignalHeader& pulseSignalHdr,
   Pulse pulses[], const string& location)
{
   const string methodName("record(...pulseSignal...)");

   DbTableKeyId dbSignalTableId(0); 

   try
   {
      if (!getDbActivityUnitId())
      {
	 stringstream strm;
	 strm << methodName
	      << ": Uninitialized dbActivityUnitId. "
	      << endl;
	 throw SseException( strm.str(),
			     __FILE__, __LINE__,
			     SSE_MSG_DBERR, SEVERITY_WARNING );
      }

      stringstream sqlStmt;

      sqlStmt << "INSERT INTO " << signalTableName << " SET ";

      putCommonSignalInfoIntoSqlStatement(sqlStmt, pulseSignalHdr.sig,
					  PulseSigType, location);

      putConfirmationStatsIntoSqlStatement(sqlStmt, pulseSignalHdr.cfm);

      // pulse train signal description

      sqlStmt << ", pulsePeriod = "
	      << pulseSignalHdr.train.pulsePeriod

	      << ", numberOfPulses = "
	      << pulseSignalHdr.train.numberOfPulses
	    
	      << ", res = '" 
	      << SseDxMsg::resolutionToString(pulseSignalHdr.train.res)
	      << "'";

      submitDbQueryWithThrowOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

      dbSignalTableId = mysql_insert_id(dbConn_);

      // record pulses
      for (int i=0; i < pulseSignalHdr.train.numberOfPulses; ++i)
      {
	 stringstream pulseSqlStmt;

	 pulseSqlStmt << "INSERT into " << pulseTrainTableName << " SET "
		      << "signalTableId = " << dbSignalTableId;
	 
	 pulseSqlStmt.precision(PrintPrecision);
	 pulseSqlStmt.setf(std::ios::fixed);  // show all decimal places to precision
	 pulseSqlStmt << ", rfFreq = " << pulses[i].rfFreq;
	 
	 pulseSqlStmt.precision(PrintPrecision);
	 pulseSqlStmt << ", power = " << pulses[i].power
		      << ", spectrumNumber = " << pulses[i].spectrumNumber
		      << ", binNumber = " << pulses[i].binNumber
		      << ", pol = '" 
		      << SseMsg::polarizationToString(pulses[i].pol)
		      << "'"
		      << " ";
	 
	 // debug
	 //cout << "pulseSqlStmt: " << pulseSqlStmt.str() << endl;
	 
	 submitDbQueryWithThrowOnError(dbConn_, pulseSqlStmt.str(), methodName, __LINE__);

      }

   }
   catch (SseException &except)
   {
      SseMessage::log(MsgSender,
                      getActivityId(), except.code(),
                      except.severity(), 
                      except.descrip(),
                      except.sourceFilename(), except.lineNumber());
   }

   return dbSignalTableId;

}

void ActivityUnitImp::recordCandidate(const CwPowerSignal& cwPowerSignal,
				      const string& location)
{
   const string & signalTableName(CandidateSignalTableName);

   record(signalTableName, cwPowerSignal, location);
}

void ActivityUnitImp::recordSignal(const CwPowerSignal& cwPowerSignal,
				 const string& location)
{
   const string & signalTableName(SignalTableName);

   updateObsSummarySignalCounts(cwPowerSignal.sig.reason);

   record(signalTableName, cwPowerSignal, location);
}


void ActivityUnitImp::record(const string &signalTableName,
			     const CwPowerSignal& cwPowerSignal,
			     const string& location)
{
   const string methodName("record(...CwPowerSignal...)");

   try
   {
      if (!getDbActivityUnitId())
      {
	 stringstream strm;
	 strm << "::record(CwPowerSignal) "
	      << "Uninitialized dbActivityUnitId. "
	      << endl;
	 throw SseException( strm.str(),
			     __FILE__, __LINE__,
			     SSE_MSG_DBERR, SEVERITY_WARNING );
      }

      stringstream sqlStmt;

      sqlStmt << "INSERT INTO " << signalTableName << " SET ";

      putCommonSignalInfoIntoSqlStatement(sqlStmt, cwPowerSignal.sig,
					  CwPowerSigType, location);

      submitDbQueryWithThrowOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

      // TBD add error check
      //  unsigned int dbSigDesId = 

      mysql_insert_id(dbConn_);
  
   }
   catch (SseException &except)
   {
      SseMessage::log(MsgSender,
                      getActivityId(), except.code(),
                      except.severity(), 
                      except.descrip(),
                      except.sourceFilename(), except.lineNumber());
   }


}

void ActivityUnitImp::putConfirmationStatsIntoSqlStatement(
   ostream & sqlStmt, 
   const ConfirmationStats & cfm)
{
   sqlStmt.precision(PrintPrecision);
    
   sqlStmt << ", pfa = " << ValidPfa(cfm.pfa)
	   << ", snr = " << cfm.snr;

}

void ActivityUnitImp::putCommonSignalInfoIntoSqlStatement(
   ostream & sqlStmt, 
   const SignalDescription &sig,
   const string & sigTypeString,
   const string & location)
{
   sqlStmt << " activityId = " << getActivityId()
	   << ", dbActivityUnitId = " << getDbActivityUnitId()
	   << ", location = '" << location << "'"
	   << ", targetId = " << targetId_
	   << ", beamNumber = " << beamNumber_
	   << ", type = '" << sigTypeString << "'"
	   << " ";

   putSignalDescriptionIntoSqlStatement(sqlStmt, sig);

}

// No more Coherent Segments
DbTableKeyId ActivityUnitImp::recordCandidate(
   const CwCoherentSignal& cwCoherentSignal,
   const string& location)
{
   const string methodName("recordCandidate(CwCoherentSignal...)");
   const string & signalTableName(CandidateSignalTableName);
   const string segmentTableName("CandidateCoherentSegments"); 

   DbTableKeyId dbSignalTableId(0);

   try
   {
      if (!getDbActivityUnitId()) 
      {
	 stringstream strm;
	 strm << "::record(CwCoherentSignal) "
	      << "Uninitialized dbActivityUnitId. "
	      << endl;
	 throw SseException( strm.str(),
			     __FILE__, __LINE__,
			     SSE_MSG_DBERR, SEVERITY_WARNING );
      }

      stringstream sqlStmt;

      sqlStmt << "INSERT INTO " << signalTableName << " SET ";

      putCommonSignalInfoIntoSqlStatement(sqlStmt, cwCoherentSignal.sig,
					  CwCohSigType, location);

      putConfirmationStatsIntoSqlStatement(sqlStmt, cwCoherentSignal.cfm);
      // number of coherent segments
      sqlStmt << ", nSegments = "
	      << cwCoherentSignal.nSegments;

      submitDbQueryWithThrowOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

      dbSignalTableId = mysql_insert_id(dbConn_);

// No more Coherent Segments
#ifdef coherentsegments
      // record coherent segments
      for (int i=0; i < cwCoherentSignal.nSegments && i < MAX_CW_COHERENT_SEGMENTS;
	   ++i) 
      {
	 stringstream segSqlStmt;
	 segSqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

	 segSqlStmt << "INSERT INTO " << segmentTableName << " SET "
		  << "signalTableId = " << dbSignalTableId;

	 segSqlStmt.precision(PrintPrecision);
	 segSqlStmt << ", rfFreq = " 
		  << cwCoherentSignal.segment[i].path.rfFreq;
	    
	 segSqlStmt.precision(PrintPrecision);
	 segSqlStmt << ", drift = " 
		  << cwCoherentSignal.segment[i].path.drift
		  << ", width = " 
		  << cwCoherentSignal.segment[i].path.width
		  << ", power = " 
		  << cwCoherentSignal.segment[i].path.power
		  << ", pfa = " 
		  << ValidPfa(cwCoherentSignal.segment[i].pfa)
		  << ", snr = " << cwCoherentSignal.segment[i].snr;

	 submitDbQueryWithThrowOnError(dbConn_, segSqlStmt.str(), methodName, __LINE__);

      }
#endif
   }
   catch (SseException &except)
   {
      SseMessage::log(MsgSender,
                      getActivityId(), except.code(),
                      except.severity(), 
                      except.descrip(),
                      except.sourceFilename(), except.lineNumber());
   }
    
   return dbSignalTableId;
    
}

void ActivityUnitImp::recordObsHistoryInDatabase()
{
   const string methodName("recordObsHistoryInDatabase");
   stringstream sqlStmt;

   sqlStmt.precision(PrintPrecision); 
   sqlStmt.setf(std::ios::fixed);      // show all decimal places up to precision

   // TBD: use the 'dxTuned' freq instead of the requested one
   // as the basis for the lower & upper dx freq limits

   TargetId primaryTargetId(getObsAct()->getActParameters().getPrimaryTargetId());

   sqlStmt << "update ActivityUnits set "
	   << "  dxNumber =  " << dxProxy_->getNumber()
	   << ", dxLowFreqMhz = " << dxBandLowerFreqLimitMHz_
	   << ", dxHighFreqMhz = " <<  dxBandUpperFreqLimitMHz_
	   << ", primaryTargetId = '" << primaryTargetId << "'"
	   << " where id = " << getDbActivityUnitId()
	   << " ";

   submitDbQueryWithLoggingOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

}



void ActivityUnitImp::recordDxSettingsInDatabase()
{
   const string methodName("recordDxSettingsInDatabase");

   try
   {
      stringstream sqlStmt;
	  
      sqlStmt << prepareDatabaseRecordStatement();

      submitDbQueryWithThrowOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

      /* WARNING: this must be the only way dbActivityUnitId_ is set */
      dbActivityUnitId_ = mysql_insert_id(dbConn_);

   }
   catch (SseException &except)
   {
      SseMessage::log(MsgSender,
                      getActivityId(), except.code(),
                      except.severity(), 
                      except.descrip(),
                      except.sourceFilename(), except.lineNumber());
   }

}

const string ActivityUnitImp::prepareDatabaseRecordStatement()
{
   stringstream sqlStmt;

   sqlStmt << "INSERT INTO ActivityUnits SET "
	   << "activityId = " << getObsAct()->getId()

	   << ", startOfDataCollection = '" 
	   << SseUtil::isoDateTimeWithoutTimezone(
	      getObsAct()->getStartTime())
	   << "' ";
	
   RecordDxActivityParameters
      recordDxActivityParameters("DxActivityParameters",
				      dxActParam_);
   unsigned int dxActId =
      recordDxActivityParameters.record(dbConn_);

   sqlStmt << ", dxActivityParametersId = " << dxActId;
	
   RecordDxIntrinsics recordDxIntrinsics("DxIntrinsics",
					   dxProxy_->getIntrinsics());
	
   unsigned int dxIntrinsicsId =
      recordDxIntrinsics.record(dbConn_);

   sqlStmt << ", dxIntrinsicsId = " << dxIntrinsicsId
	   << ", targetId = " << targetId_
	   << ", beamNumber = " << beamNumber_;

   return sqlStmt.str();
}


void ActivityUnitImp::recordDetectionStats(DxProxy *proxy,
					   const DetectionStatistics &stats) 
{
   const string methodName("recordDetectionStats");
   const string tableName("DetectionStats");
  
   stringstream sqlStmt;
   sqlStmt << "INSERT INTO " << tableName  << " SET "
	   << " actId = " << getActivityId()
	   << ", actUnitId = " << getId()
	   << ", dxNumber = " << proxy->getNumber()
	   << ", totalCandidates = " << stats.totalCandidates  
	   << ", cwCandidates = " << stats.cwCandidates      
	   << ", pulseCandidates = " << stats.pulseCandidates 
	   << ", candidatesOverMax = " << stats.candidatesOverMax 
	   << ", totalSignals = " << stats.totalSignals
	   << ", cwSignals = " << stats.cwSignals 
	   << ", pulseSignals = " << stats.pulseSignals 
	   << ", leftCwHits = " << stats.leftCwHits     
	   << ", rightCwHits = " << stats.rightCwHits      
	   << ", leftCwClusters = " << stats.leftCwClusters
	   << ", rightCwClusters = " << stats.rightCwClusters 
	   << ", totalPulses = " << stats.totalPulses    
	   << ", leftPulses = " << stats.leftPulses      
	   << ", rightPulses = " << stats.rightPulses    
	   << ", triplets = " << stats.triplets         
	   << ", pulseTrains = " << stats.pulseTrains   
	   << ", pulseClusters = " << stats.pulseClusters
	   << " ";

   // debug
   // cout << "sql detection stats: " << sqlStmt.str() << endl;

   submitDbQueryWithLoggingOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

}



void ActivityUnitImp::recordBadBandInDb(DxProxy *proxy,
					const CwBadBand & cwBadBand) 
{
   const string methodName("recordBadBandInDb");
   string tableName("CwBadBands");
  
   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

   sqlStmt << "INSERT INTO " << tableName  << " SET "
	   << " actId = " << getActivityId()
	   << ", dxNumber = " << proxy->getNumber()
	   << ", centerFreq = " << cwBadBand.band.centerFreq
	   << ", bandwidth = " << cwBadBand.band.bandwidth
	   << ", pol = '" << SseMsg::polarizationToString(cwBadBand.pol)
	   << "'"
	   << ", paths = " <<  cwBadBand.paths
	   << ", maxPathCount = " << cwBadBand.maxPathCount
	   << ", rfFreq = " << cwBadBand.maxPath.rfFreq
	   << ", drift = " << cwBadBand.maxPath.drift
	   << ", width = " << cwBadBand.maxPath.width
	   << ", power = " << cwBadBand.maxPath.power
	   << " ";

   submitDbQueryWithLoggingOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

}

void ActivityUnitImp::recordBadBandInDb(DxProxy *proxy,
					const PulseBadBand & pulseBadBand) 
{
   const string methodName("recordBadBandInDb");
   string tableName("PulseBadBands");
  
   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

   sqlStmt << "INSERT INTO " << tableName  << " SET "
	   << " actId = " << getActivityId()
	   << ", dxNumber = " << proxy->getNumber()
	   << ", centerFreq = " << pulseBadBand.band.centerFreq
	   << ", bandwidth = " << pulseBadBand.band.bandwidth
	   << ", res = '" << SseDxMsg::resolutionToString(pulseBadBand.res)
	   << "'"
	   << ", pol = '" << SseMsg::polarizationToString(pulseBadBand.pol)
	   << "'"
	   << ", pulses = " <<  pulseBadBand.pulses
	   << ", maxPulseCount = " << pulseBadBand.maxPulseCount
	   << ", triplets = " << pulseBadBand.triplets
	   << ", maxTripletCount = " << pulseBadBand.maxTripletCount
	   << ", tooManyTriplets = " << BoolToQuotedYesNo(pulseBadBand.tooManyTriplets)
	   << " ";

   submitDbQueryWithLoggingOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

}

void ActivityUnitImp::recordBaselineStatsInDb(
   DxProxy *proxy, const BaselineStatistics &stats)
{
   const string methodName("recordBaselineStatsInDb");
   const string tableName("BaselineStats");
  
   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

   sqlStmt << "INSERT INTO " << tableName  << " SET "
	   << " actId = " << getActivityId()
	   << ", dxNumber = " << proxy->getNumber()
	   << ", mean = " << stats.mean
	   << ", stdDev = " << stats.stdDev
	   << ", blineRange = " << stats.range
	   << ", halfFrameNumber = " << stats.halfFrameNumber
	   << ", rfCenterFreqMhz = " << stats.rfCenterFreqMhz
	   << ", bandwidthMhz = " << stats.bandwidthMhz
	   << ", pol = '" << SseMsg::polarizationToString(stats.pol)
	   << "'"
	   << ", status = '" << SseDxMsg::baselineStatusToString(
	      stats.status) 
	   << "'"
	   << " ";

   submitDbQueryWithLoggingOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

}


// ---- methods called by activity  -----------------------

void ActivityUnitImp::initialize()
{
   VERBOSE2(getVerboseLevel(), "ActivityUnit initialize()" << endl;  );

   // remember that dx Act param already contains initial 
   // DxScienceDataRequest
   dxProxy_->sendDxActivityParameters(dxActParam_);
    
   // Tell the Science Data Archive which subchannel is for the Monitor
   if (dxActParam_.scienceDataRequest.scienceData.requestType == REQ_SUBCHANNEL)
	   scienceDataArchive_->setCurrentDataRequestSubchannel(
			   dxActParam_.scienceDataRequest.scienceData.subchannel);
   else 
   {
	   int subchan = getSubchannel(
			   dxActParam_.scienceDataRequest.scienceData.rfFreq);
	   scienceDataArchive_->setCurrentDataRequestSubchannel(subchan);
   }

   if (getObsAct()->getDbParameters().useDb())
   {
      recordDxSettingsInDatabase();
      recordObsHistoryInDatabase();
   }
      
}



void ActivityUnitImp::dxScienceDataRequest(
   const DxScienceDataRequest &dataRequest)
{
   VERBOSE2(getVerboseLevel(),
	    "ActivityUnit::dxScienceDataRequest" << endl;);

   dxProxy_->dxScienceDataRequest(dataRequest);
}


void ActivityUnitImp::setStartTime(const StartActivity &startAct)
{
   VERBOSE2(getVerboseLevel(),
	    "ActivityUnit setStartTime" << endl;);

   // pass on the start time to the dxproxy
   dxProxy_->setStartTime(getActivityId(), startAct);

   if (getObsAct()->getDbParameters().useDb())
   {
      updateStartTimeAndDxSkyFreqInDb(startAct);
   }
    
}

void ActivityUnitImp::updateStartTimeAndDxSkyFreqInDb(
   const StartActivity& startAct)
{
   const string methodName("updateStartTimeAndDxSkyFreqInDb");

   if (!getDbActivityUnitId())
   {
      stringstream strm;
      strm << " Uninitialized DB Activity ID" << endl;

      SseMessage::log(MsgSender,
                      getActivityId(),
                      SSE_MSG_UNINIT_DB_ID, SEVERITY_WARNING, 
                      strm.str(), __FILE__, __LINE__);
      return;
   }

   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);   // show N places after the decimal
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision
  
   sqlStmt << "UPDATE ActivityUnits SET "
	   << " startOfDataCollection = '" << startAct.startTime << "', "
	   << "dxTuneFreqActual = " << actualDxTunedFreqMhz_ << ", "
	   << "dxTuneFreq = " << dxProxy_->getDxSkyFreq()
	   << " where id = " << getDbActivityUnitId()
	   << " ";

   submitDbQueryWithLoggingOnError(dbConn_, sqlStmt.str(), methodName, __LINE__);

}

void ActivityUnitImp::sendStopMsgToDx()
{
   dxProxy_->stopDxActivity(getActivityId());
}

void ActivityUnitImp::stop(DbParameters &callerDbParam)
{
   VERBOSE2(getVerboseLevel(), "ActivityUnitImp::stop" << endl);   

   // Disconnect from the dx proxy before sending the 
   // stop (which is done in terminateActivityUnit), in order
   // to simplify wrapping up.

   stopCommandReceived_.set(true);

   detachSelfFromDxProxy();

   SseException except("stop command received");
   terminateActivityUnit(callerDbParam, except);
 
}

void ActivityUnitImp::shutdown()
{
   VERBOSE2(getVerboseLevel(), "ActivityUnitImp::shutdown" << endl);   

   // pass on the message to the dxproxy
   dxProxy_->shutdown();
}

void ActivityUnitImp::resetSocket()
{
   VERBOSE2(getVerboseLevel(), "ActivityUnitImp::resetSocket" << endl);   

   // pass on the message to the dxproxy
   dxProxy_->resetSocket();
}


string ActivityUnitImp::getDxName()
{
   return dxProxy_->getName();
}

int ActivityUnitImp::getDxNumber()
{
   return dxProxy_->getNumber();
}

// ---- methods called by dxProxy -----------------------



void ActivityUnitImp::dxTuned(DxProxy* dx, const DxTuned &dxTuned)
{
   VERBOSE2(getVerboseLevel(),
	    "ActivityUnitImp::dxTuned()" << endl;);

   ActUnitSummaryStrm()
      << dx->getName() << endl
      << dxTuned;

   actualDxTunedFreqMhz_ = dxTuned.dxSkyFreq;

   // Tuned freq should be within a few Khz of the requested freq
   // (exact value tbd)
   const double SkyFreqTuneTolMhz(0.005000);

   double diffMhz = fabs(dx->getDxSkyFreq() - dxTuned.dxSkyFreq);
   if (diffMhz	> SkyFreqTuneTolMhz)
   {
      stringstream strm;
      strm.precision(PrintPrecision);
      strm.setf(std::ios::fixed);  // show all decimal places up to precision

      strm  <<  "Sky freq requested (" 
	    << dx->getDxSkyFreq() << " MHz) "
	    << "and tuned (" << dxTuned.dxSkyFreq
	    << " MHz) differ by " << diffMhz << " MHz, which is greater than"
	    << " expected" << endl;

      SseMessage::log(dx->getName(), getActivityId(),
                      SSE_MSG_DX_TUNE_DIFF, SEVERITY_WARNING, 
                      strm.str(), __FILE__, __LINE__);
	
      ActUnitSummaryStrm() << strm.str();
   } 

   getObsAct()->activityUnitReady(this);

}

void ActivityUnitImp::dataCollectionStarted(DxProxy* dx)
{
   VERBOSE2(getVerboseLevel(),
	    "ActUnit::dataCollectionStarted()" << endl;);

   ActUnitSummaryStrm() << "DataCollectionStarted: "
                        << SseUtil::currentIsoDateTime() << endl;

   getObsAct()->dataCollectionStarted(this);

}

void ActivityUnitImp::dataCollectionComplete(DxProxy* dx)
{
   VERBOSE2(getVerboseLevel(),
	    "ActUnit::dataCollectionComplete()" << endl;);

   ActUnitSummaryStrm() << "DataCollectionComplete: "
                        << SseUtil::currentIsoDateTime() << "\n"
                        << endl;

   getObsAct()->dataCollectionComplete(this);
   if(zxMode_)getObsAct()->signalDetectionStarted(this);
}


void ActivityUnitImp::sendBaseline(DxProxy* dx,
				   const BaselineHeader &hdr,
				   BaselineValue valueArray[])
{
   try
   {
      getScienceDataArchive()->storeBaseline(hdr, valueArray);
   }
   catch (SseException &except)
   {
      terminateActivityUnit(dbParam_, except);
   }
}

void ActivityUnitImp:: sendComplexAmplitudes(
   DxProxy* dx,
   const ComplexAmplitudeHeader &hdr,
   SubchannelCoef1KHz subchannelArray[])
{
   try
   {
      getScienceDataArchive()->storeComplexAmplitudes(hdr, subchannelArray);
   } 
   catch (SseException &except)
   {
      terminateActivityUnit(dbParam_, except);
   }
}

void ActivityUnitImp::logCompactBaselineStats(const BaselineStatistics &stats)
{
   ActUnitSummaryStrm()
      << "basestats:"
      << " hf#: " << stats.halfFrameNumber
      << " pol: " << SseMsg::polarizationToSingleUpperChar(stats.pol) 
      << " mean: " << stats.mean 
      << " stdDev: " << stats.stdDev 
      << " range: " << stats.range 
      << " status: " << SseDxMsg::baselineStatusToString(stats.status)
      << endl;
}

void ActivityUnitImp::sendBaselineStatistics(
   DxProxy *dx,
   const BaselineStatistics &baselineStats)
{
   VERBOSE2(getVerboseLevel(),
	    "ActUnit::sendBaselineStatistics from "
	    << dx->getName() << endl;);

   logCompactBaselineStats(baselineStats);

   if (getObsAct()->getDbParameters().useDb())
   {
      recordBaselineStatsInDb(dx, baselineStats);
   }


}


void ActivityUnitImp::baselineWarningLimitsExceeded(
   DxProxy *dx,
   const BaselineLimitsExceededDetails &details)
{
   stringstream strm;
   strm << "Baseline warning limits exceeded: "
	<< dx->getName() << " "
	<< SseMsg::polarizationToSingleUpperChar(details.pol) << " "
	<< details.description << endl;

   SseMsgCode sseMsgCode(SSE_MSG_BASELINE_RW);
   if (details.pol == POL_LEFTCIRCULAR)
   {
      sseMsgCode = SSE_MSG_BASELINE_LW;
   }

   SseMessage::log(dx->getName(), getActivityId(),
                   sseMsgCode, SEVERITY_WARNING, strm.str(),
                   __FILE__, __LINE__);

   // TBD store in database?
}


void ActivityUnitImp::baselineErrorLimitsExceeded(
   DxProxy *dx,
   const BaselineLimitsExceededDetails &details)
{
   stringstream strm;

   strm << "Baseline Error limits exceeded: "
	<< dx->getName() << " "
	<< SseMsg::polarizationToSingleUpperChar(details.pol) << " "
	<< details.description << endl;

   SseMsgCode sseMsgCode(SSE_MSG_BASELINE_RE);
   if (details.pol == POL_LEFTCIRCULAR)
   {
      sseMsgCode = SSE_MSG_BASELINE_LE;
   }

   SseMessage::log(dx->getName(), getActivityId(),
                   sseMsgCode, SEVERITY_ERROR, strm.str(),
                   __FILE__, __LINE__);

   // TBD additional error handling

   // TBD store in database?
}


void ActivityUnitImp::signalDetectionStarted(DxProxy* dx)
{
   VERBOSE2(getVerboseLevel(),
	    "ActUnit::signalDetectionStarted()" << endl;);

   ActUnitSummaryStrm() << "SignalDetectionStarted: "
                        << SseUtil::currentIsoDateTime() << endl;

   getObsAct()->signalDetectionStarted(this);

}

void ActivityUnitImp::signalDetectionComplete(DxProxy* dx)
{
   VERBOSE2(getVerboseLevel(),
	    "ActUnit::signalDetectionComplete()" << endl;);

   ActUnitSummaryStrm() << "SignalDetectionComplete: "
                        << SseUtil::currentIsoDateTime()
                        << "\n" << endl;

   if (zxMode_)
   {
   getObsAct()->doneSendingCwCoherentSignals(this);
   getObsAct()->doneSendingCandidateResults(this);
   // send signal detection complete after processing SetiLive Candidates
   }
   else
   {
   getObsAct()->signalDetectionComplete(this);
   }

}

bool ActivityUnitImp::signalPassedOffActNullBeamSnrTest(
   SignalDescription &descrip,
   ConfirmationStats &cfm)
{
   Assert(actOpsBitEnabled(OFF_OBSERVATION));

   /*
     Match this signal report to the original followup request,
     based on the original signal id number.
     This is an OFF, so only candidates previously seen by this dx
     should be in the list.
     Compare new SNR to that of previous ON detection.
     It has to be reduced by at least the null depth to remain
     a viable candidate.

     TBD:
     - how handle signal strength variability due to
     inherent signal characteristics, scintillation, etc.
     - handle change in signal type, e.g., orig signal was CW but
     followup found it in the pulse detector
   */

   bool passed=false;
   try
   {
      //cout << "***looking up " << descrip << endl;

      FollowUpSignalInfo & origFollowUpSigInfo = lookUpFollowUpSignalById(
         descrip.origSignalId.number);

      // TBD error handling if not found.
#if 0
      cout << "dx " << getDxNumber() 
           << " signal returned by dx: " 
           << descrip 
           << " original followup request " 
           << origFollowUpSigInfo.followUpSignal
           << " orig signal id# " 
           << descrip.origSignalId.number 
           << " followUpSigSnr: " << origFollowUpSigInfo.cfm.snr << endl;
#endif

      double snrRatio = origFollowUpSigInfo.cfm.snr / cfm.snr;

      string result;
	    // for a pfa of -70, the average snr is .400
     if ( origFollowUpSigInfo.cfm.snr <= .400 &&  
		     origFollowUpSigInfo.cfm.snr >= .150)
	    {
		    // for a pfa of -20, the average snr is .100
		    // above -20 is had to tell from the noise.
	       if (cfm.snr > .100 )
            	{
                result = "Signal is RFI, too strong.";
            	}
            	else
            	{
         	passed = true;
               	result = "Signal is potential candidate.";
            	}
	    }
      else if (snrRatio < nullDepthLinear_)
      {
         result = "Signal is RFI, too strong.";
      }
      else
      {
         /* Signal was seen, but the signal strength
            was reduced by the amount expected due to the null.
         */
         passed = true;
         result = "Signal is potential candidate.";
      }

      SseArchive::SystemLog()
         <<"Act " << getActivityId() << ":"
         << " OFF act null beam compare:"
         << " dx" << getDxNumber() 
         << " sig#: " << descrip.signalId.number
         << " vs. earlier act sig#: " 
         << origFollowUpSigInfo.followUpSignal.origSignalId.number
         << " snrRatio: " << snrRatio 
         << " (" << SseUtil::linearRatioToDb(snrRatio) << " dB). "
         << result << endl;

   }
   catch(SseException & except)
   {
      SseArchive::ErrorLog() << "Act " << getActivityId() << ":"
                             << except << endl;
   }

   return passed;
}



/*
 Reclassify the candidate signal, based on the signal description,
 and whether this is an ON or OFF observation.
 This is used for CW Coherent and Pulse signals (not CW Power)
 (TBD add sig type assert)
*/
void ActivityUnitImp::reclassifyCandidateSignal(
   SignalDescription &descrip,
   ConfirmationStats &cfm)
{
   ActUnitSummaryStrm() << "original dx signal report: " 
			      << descrip <<endl;

   if (actOpsBitEnabled(OFF_OBSERVATION))
   {
      if (descrip.sigClass == CLASS_CAND || 
          descrip.reason == FAILED_COHERENT_DETECT)
      {
          /*
            The signal was seen (regardless of whether it passed the
            coherent threshhold), so reclassify it as RFI.
          */
	 descrip.sigClass = CLASS_RFI;
	 descrip.reason = SEEN_OFF;

         /*
           However; if nulls are in use, and the signal strength
           was reduced by an amount compatible with the null depth,
           then turn it back into a candidate.
         */
         if (usingOffActNull_)
         {
            if (signalPassedOffActNullBeamSnrTest(descrip, cfm))
            {
               descrip.sigClass = CLASS_CAND;
               descrip.reason = RECONFIRM;
            }
         }
      }
      else if (descrip.sigClass == CLASS_RFI)
      {
         if (descrip.reason == NO_SIGNAL_FOUND)
	 {
	    // The dx didn't see the signal 
	    // so reclassify it back as a candidate

	    descrip.sigClass = CLASS_CAND;
	    descrip.reason = RECONFIRM;
	 }

	 // If it's RFI for any other reason (e.g. zero drift)
	 // then just keep its reason & classification as is.
      }
      else 
      {
	 stringstream strm;
	 strm  << "OFF: "
	       << "unexpected signal classification:" <<  endl
	       << descrip << endl;

	 SseMessage::log(MsgSender, getActivityId(),
                         SSE_MSG_BAD_SIG_CLASS, SEVERITY_WARNING,
                         strm.str(), __FILE__, __LINE__);

	 // unexpected classification
	 // Error handling TBD
      }
	
   }
   else if (actOpsBitEnabled(ON_OBSERVATION))
   {
      if (descrip.sigClass == CLASS_CAND)
      {
	 // dx original reason should be PASSED_COHERENT_DETECTION (cw)
	 // or PASSED_POWER_THRESH (pulse)
	 if (! actOpsBitEnabled(MULTITARGET_OBSERVATION))
	 {
	    descrip.reason = RECONFIRM;
 }
      }
      else if (descrip.sigClass == CLASS_RFI)
      {
	 // no reclassification

      }
      else
      {
	 stringstream strm;
	 strm << "ON"
	      << " unexpected signal classification: " 
	      << descrip << endl;
	 SseMessage::log(MsgSender, getActivityId(),
                         SSE_MSG_BAD_SIG_CLASS, SEVERITY_WARNING, 
                         strm.str(), __FILE__, __LINE__);

	 // tbd error handling, unexpected sigClass
      }

   }
   else  // regular observation
   {
      if (descrip.sigClass == CLASS_CAND)
      {
	 if (! actOpsBitEnabled(MULTITARGET_OBSERVATION))
	 {
	    // change classification from "passed thresh" to confirmed
	    descrip.reason = CONFIRM;
	 }
      }
	
   }
   ActUnitSummaryStrm() << "sse reclassified dx report: " 
			      << descrip <<endl;


}



void ActivityUnitImp::beginSendingCandidates(
   DxProxy* dx, const Count &count)
{
   // do nothing
}


void ActivityUnitImp::sendCandidatePulseSignal(
   DxProxy* dx, const PulseSignalHeader &pulseSignalHdr,
   Pulse pulses[])
{
   VERBOSE2(getVerboseLevel(),
	    "Candidate pulse signal received" << endl;);

   getSigReportTxtStrm() << "\n__Candidate__" << endl;
   SseDxMsg::printPulseSignal(getSigReportTxtStrm(), pulseSignalHdr, pulses);

   // Work with a copy of the signal so we can modify its classification.
   PulseSignalHeader hdr = pulseSignalHdr;

   reclassifyCandidateSignal(hdr.sig, hdr.cfm);
   if (! actOpsBitEnabled(MULTITARGET_OBSERVATION))
   {
      // This is not a multitarget observation, so archive request
      // can be made immediately
      
      determineArchiveRequest(dx, hdr.sig);
   }

   if ( !actOpsBitEnabled(MULTITARGET_OBSERVATION) && 
	(hdr.sig.sigClass == CLASS_CAND))
   {
      getObsSummaryStats().confirmedPulseCandidates++;
   }

   getCondensedCandidateReport()->addSignal(hdr);
   getExpandedCandidateReport()->addSignal(hdr, pulses);

   getObsSummaryStats().allPulseCandidates++;

   // store a copy of pulseSignal for use in later archive requests
   // temp disable.  expect the dx to autoarchive for now.
   //confirmedPulseSigList_.push_back(pulseSignal);

   // ----- database -----

   if (getObsAct()->getDbParameters().useDb())
   {
      DbTableKeyId dbTableKeyId = 
	 recordCandidate(hdr, pulses, siteName_);

      savePulseCandSigInfo(dbTableKeyId, hdr.sig, hdr.cfm);
   }
}



void ActivityUnitImp::sendCandidateCwPowerSignal(DxProxy* dx,
						 const CwPowerSignal &cwPowerSignal)
{
   // Use local copy of the signal since its classification might change 
   CwPowerSignal signal = cwPowerSignal;

   // Only OFFs potentially need to be reclassified 
   if (actOpsBitEnabled(OFF_OBSERVATION))
   {
      /*
        If the signal was not seen at all,
        count it as a confirmed cw candidate.
        There will not be a subsequent cw coherent report.
      */
      if (signal.sig.sigClass == CLASS_RFI &&
          signal.sig.reason == NO_SIGNAL_FOUND)
      {
         signal.sig.sigClass = CLASS_CAND;
         signal.sig.reason = RECONFIRM;

         getObsSummaryStats().confirmedCwCandidates++;
      }

      // Dxs generate an error if asked to archive
      // candidates that weren't seen, so don't make
      // an archive request here.
   }

   /*
     Log the signal.  If it was seen at all, then
     any remaining archiving and other reclassification decisions 
     will wait until after the subsequent cw coherent report arrives.
   */
   
   getSigReportTxtStrm() << "\n__Candidate__" << endl;
   getSigReportTxtStrm() << signal;
   
   getCondensedCandidateReport()->addSignal(signal);
      
   getObsSummaryStats().allCwCandidates++;
   
   // ----- database -----

   if (getObsAct()->getDbParameters().useDb())
   {
      recordCandidate(signal, siteName_);
   }
      
}

void ActivityUnitImp::doneSendingCandidates(DxProxy* dx)
{
   // do nothing
}



void ActivityUnitImp::beginSendingSignals(DxProxy* dx, const DetectionStatistics &stats)
{
   ActUnitSummaryStrm() << "\n" 
                        << stats << endl;

   // store stats in database
   if (getObsAct()->getDbParameters().useDb())
   {
      recordDetectionStats(dx, stats);
   }


}



void ActivityUnitImp::sendPulseSignal(DxProxy* dx,
				      const PulseSignalHeader &origHdr,
				      Pulse pulses[])
{
   PulseSignalHeader hdr(origHdr);
    
   if (actOpsBitEnabled(CLASSIFY_ALL_SIGNALS_AS_RFI_SCAN))
   {
      hdr.sig.sigClass = CLASS_RFI;
      hdr.sig.reason = RFI_SCAN;
   }

   getSigReportTxtStrm() << "\n";
   SseDxMsg::printPulseSignal(getSigReportTxtStrm(), hdr, pulses);

   getCondensedAllSignalReport()->addSignal(hdr);
   getExpandedAllSignalReport()->addSignal(hdr, pulses);

   getObsSummaryStats().pulseSignals++;

   // ----- database -----

   if (getObsAct()->getDbParameters().useDb())
   {
      recordSignal(hdr, pulses, siteName_);
   }

}

void ActivityUnitImp::sendCwPowerSignal(DxProxy* dx,
					const CwPowerSignal &cwPowerSignal)
{

   // use local copy of the signal since its classifcation might change 
   CwPowerSignal signal = cwPowerSignal;

   if (actOpsBitEnabled(CLASSIFY_ALL_SIGNALS_AS_RFI_SCAN))
   {
      signal.sig.sigClass = CLASS_RFI;
      signal.sig.reason = RFI_SCAN;
   } 


#ifdef FIND_TEST_SIGNAL
// Disabled since not currently used
   // Check for test signal.  Note that the classification might change.

   if (testSigParameters_.getCheckForTestSignal() && 
       matchCwPowerTestSignal(signal, testSigParameters_, dxActParam_.ifcSkyFreq))
   {
      // TBD better results handling

      stringstream strm;
      strm.precision(PrintPrecision);           // show N places after the decimal
      strm.setf(std::ios::fixed);  // show all decimal places up to precision

      strm << "Test Signal Found:" << endl
	   << signal << endl;

      SseArchive::SystemLog() << strm.str();
      ActUnitSummaryStrm() << strm.str();

      getObsSummaryStats().testSignals++;
   }

#endif

   getSigReportTxtStrm() << "\n" << signal;

   getCondensedAllSignalReport()->addSignal(signal);

   getObsSummaryStats().cwSignals++;

   // ----- database -----

   if (getObsAct()->getDbParameters().useDb()) 
   {
      recordSignal(signal, siteName_);
   }

}

void ActivityUnitImp::doneSendingSignals(DxProxy* dx)
{
   // do nothing
}


void ActivityUnitImp::beginSendingCwCoherentSignals(DxProxy* dx,
						    const Count &count)
{
   // do nothing
}

void ActivityUnitImp::sendCwCoherentSignal(
   DxProxy* dx, const CwCoherentSignal &cwCoherentSignal)
{
   // Work with a copy of the signal so we can modify its classification.
   CwCoherentSignal signal = cwCoherentSignal;

   reclassifyCandidateSignal(signal.sig, signal.cfm);
   
   if (! actOpsBitEnabled(MULTITARGET_OBSERVATION) &&
       ! actOpsBitEnabled(FORCE_ARCHIVING_AROUND_CENTER_TUNING))
   {
      // There are no multitarget results to consider, so archive request
      // can be made immediately.
      // Note: theoretically it should be possible to do this
      // for the FORCE_ARCHIVING_AROUND_CENTER_TUNING mode, but
      // in practice it seems to make the dx ignore archiving for
      // all the secondary candidate results.
      
      determineArchiveRequest(dx, signal.sig);
   }

   if (signal.sig.sigClass == CLASS_CAND)
   {
      VERBOSE2(getVerboseLevel(),
	       "CwCoherent Signal confirmed" << endl;);

      getObsSummaryStats().passCwCohDetCandidates++;

      if (! actOpsBitEnabled(MULTITARGET_OBSERVATION))
      {
	 getObsSummaryStats().confirmedCwCandidates++;
      }
   }
    
   getSigReportTxtStrm() << "\n" << signal;

   getCondensedCandidateReport()->addSignal(signal);

   // ----- database -----

   if (getObsAct()->getDbParameters().useDb()) 
   {
      DbTableKeyId dbTableKeyId = 
	 recordCandidate(signal, siteName_);

      saveCwCohCandSigInfo(dbTableKeyId, signal.sig, signal.cfm);
   }


}

void ActivityUnitImp::doneSendingCwCoherentSignals(DxProxy* dx)
{
   VERBOSE2(getVerboseLevel(),
	    "ActUnit::doneSendingCwCoherentSignals()" 
	    << dx->getName() << endl;);

   getObsAct()->doneSendingCwCoherentSignals(this);
}



void ActivityUnitImp::beginSendingCandidateResults(
   DxProxy* dx, const Count &count)
{
   VERBOSE2(getVerboseLevel(), "beginSendingCandidateResults" 
	    <<  " from " << dx->getName() << " count: " << count.count << endl;);

   Assert(actOpsBitEnabled(MULTITARGET_OBSERVATION)
	  || actOpsBitEnabled(FORCE_ARCHIVING_AROUND_CENTER_TUNING));

   // Define a report separator.
   // Note: keep the phrase "Dx Name" in the text below, 
   // so that the dx comparison utility can key off of it to
   // ignore dx name differences in the report.

   string text("\n# ===== Secondary Candidate Results =============\n");

   getCondensedCandidateReport()->addText(text);
   getExpandedCandidateReport()->addText(text);

};

void ActivityUnitImp::sendPulseCandidateResult(DxProxy* dx,
					       const PulseSignalHeader &pulseSignalHdr,
					       Pulse pulses[])
{
   VERBOSE2(getVerboseLevel(),
	    "pulseCandidateResult received" << endl;);

   Assert(actOpsBitEnabled(MULTITARGET_OBSERVATION));

   ostream &sigReportStrm = getSigReportTxtStrm();

   sigReportStrm << "\n__Candidate Result__" << endl;
   SseDxMsg::printPulseSignal(sigReportStrm, pulseSignalHdr, pulses);

   // Work with a copy of the signal so we can modify its classification.
   PulseSignalHeader hdr = pulseSignalHdr;

   // TBD call reclassifyCandidateSignal()

   if (hdr.sig.sigClass == CLASS_CAND)  
   {
      // signal was seen 
      hdr.sig.reason = SECONDARY_FOUND_SIGNAL;
   }
   else   // signal was not seen (class is RFI)
   {
      hdr.sig.sigClass = CLASS_UNKNOWN;
      hdr.sig.reason = SECONDARY_NO_SIGNAL_FOUND;
   }

   getCondensedCandidateReport()->addSignal(hdr);
   getExpandedCandidateReport()->addSignal(hdr, pulses);

   saveSecondarySignalDescrip(hdr.sig);

   // ----- database -----

   if (getObsAct()->getDbParameters().useDb()) 
   {
      recordCandidate(hdr, pulses,  siteName_);
   }

};

void ActivityUnitImp::sendCwCoherentCandidateResult(
   DxProxy* dx, const CwCoherentSignal &cwCoherentSignal)
{
   VERBOSE2(getVerboseLevel(),
	    "CwCoherentCandidateResult received" << endl;);

   Assert(actOpsBitEnabled(MULTITARGET_OBSERVATION)
	  || actOpsBitEnabled(FORCE_ARCHIVING_AROUND_CENTER_TUNING));

   ostream &sigReportStrm = getSigReportTxtStrm();

   sigReportStrm << "\n__Candidate Result__" 
		 << "\n" << cwCoherentSignal << endl;

   // Work with a copy of the signal so we can modify its classification.
   CwCoherentSignal signal = cwCoherentSignal;

   // TBD call reclassifyCandidateSignal()

   if (signal.sig.sigClass == CLASS_CAND)  
   {
      // signal was seen 
      signal.sig.reason = SECONDARY_FOUND_SIGNAL;

   }
   else   // signal was not seen (class is RFI)
   {
      signal.sig.sigClass = CLASS_UNKNOWN;
      signal.sig.reason = SECONDARY_NO_SIGNAL_FOUND;
   }

   getCondensedCandidateReport()->addSignal(signal);

   saveSecondarySignalDescrip(signal.sig);


   // ----- database -----
   if (getObsAct()->getDbParameters().useDb()) 
   {
      recordCandidate(signal, siteName_);
   }

};

void ActivityUnitImp::doneSendingCandidateResults(DxProxy* dx)
{
   VERBOSE2(getVerboseLevel(),
	    "ActivityUnitImp::doneSendingCandidateResults()" 
	    << dx->getName() << endl;);

   Assert(actOpsBitEnabled(MULTITARGET_OBSERVATION) ||
      actOpsBitEnabled(FORCE_ARCHIVING_AROUND_CENTER_TUNING));

   string text("\n# ===== Primary Candidate Resolution =============\n");

   getCondensedCandidateReport()->addText(text);

   // TBD update expanded report
   // getExpandedCandidateReport()->addText(text);

   getObsAct()->doneSendingCandidateResults(this);

};


void ActivityUnitImp::beginSendingBadBands(DxProxy *dx, const Count &count)
{  
   stringstream strm;

   strm << "\n" 
	<< "Bad Band count: " << count.count
	<< endl;

   getSigReportTxtStrm() << strm.str();
   ActUnitSummaryStrm() << strm.str();
}

void ActivityUnitImp::sendCwBadBand(DxProxy *dx, const CwBadBand &cwBadBand)
{
   getSigReportTxtStrm() << "\n" << cwBadBand;
   ActUnitSummaryStrm() << "\n" << cwBadBand;

   if (getObsAct()->getDbParameters().useDb()) 
   {
      recordBadBandInDb(dx, cwBadBand);
   }


}

void ActivityUnitImp::sendPulseBadBand(DxProxy *dx,
				       const PulseBadBand &pulseBadBand)
{
   getSigReportTxtStrm() << "\n" << pulseBadBand;
   ActUnitSummaryStrm() << "\n" << pulseBadBand;

   if (getObsAct()->getDbParameters().useDb()) 
   {
      recordBadBandInDb(dx, pulseBadBand);
   }

}

void ActivityUnitImp::doneSendingBadBands(DxProxy *dx)
{
   // do nothing
}


void ActivityUnitImp::archiveComplete(DxProxy* dx)
{
   // do nothing
}

// send archive requests for secondary candidates processed on this dx
//
void ActivityUnitImp::sendArchiveRequestsForSecondaryCandidates()
{
   for (SigDescripList::iterator it = 
	   secondarySignalDescripList_.begin();
	it != secondarySignalDescripList_.end();
	++it)
   {
      SignalDescription & descrip = *it;

      determineArchiveRequest(dxProxy_, descrip);
   }

}

void ActivityUnitImp::sendArchiveRequestsForPrimaryCandidates()
{
   for(ActivityUnitImp::CandSigInfoList::iterator candIt = 
	  candSigInfoList_.begin(); 
       candIt != candSigInfoList_.end(); ++candIt)
   {
      CandSigInfo & candSigInfo = *candIt;
      
      determineArchiveRequest(dxProxy_, candSigInfo.descrip);
   }
}




// handle premature dx disconnection
//
void ActivityUnitImp::notifyDxProxyDisconnected(DxProxy *dxProxy)
{
   Assert(dxProxy == dxProxy_);

   VERBOSE0(getVerboseLevel(),
	    "ActUnit::notifyDxProxyDisconnected() "
	    << dxProxy->getName() << endl;);

   stringstream strm;
   strm << "Dx disconnected unexpectedly: " 
	<< dxProxy->getName() << endl
	<< "ActivityUnit " << getId()
	<< " terminating." << endl;
   SseMessage::log(dxProxy->getName(), getActivityId(),
                   SSE_MSG_DX_DISCONNECT, SEVERITY_ERROR, 
                   strm.str(), __FILE__, __LINE__);

   ActUnitSummaryStrm()
      << "\n"
      << "===============" << endl
      << "ERROR: Dx disconnected unexpectedly: " 
      << dxProxy->getName() << endl
      << "ActivityUnit " << getId()
      << " terminating." << endl
      << "===============" << endl;

   SseException except(strm.str());
   terminateActivityUnit(dbParam_, except);

   // Warning ActivityUnit may be destroyed in this method.
   // Do not use ActivityUnit past this point.

}

void ActivityUnitImp::dxActivityComplete(DxProxy* dx,
					  const DxActivityStatus &status)
{
   activityUnitComplete();

   // Warning ActivityUnit may be destroyed in this method.
   // Do not use ActivityUnit past this point.

}

// internal utilities and classes
//-------------------------------


// ------------------------------------------------

LookUpCandidatesFromPrevAct::LookUpCandidatesFromPrevAct(
   ActivityUnitImp *activityUnit,
   MYSQL *callerDbConn,
   int previousActId,
   FollowUpSignalInfoList &infoList)
   :
   DbQuery(callerDbConn, activityUnit->getActivityId(), 
	   activityUnit->getVerboseLevel()),
   actUnit_(activityUnit),
   previousActId_(previousActId),
   infoList_(infoList)
{
   setNumberOfRequestedCols(nRequestedCols);
   setContext(actUnit_->getDxName());
   setSubclassName("LookUpCandidatesFromPrevAct");
}

LookUpCandidatesFromPrevAct::~LookUpCandidatesFromPrevAct()
{
}

string LookUpCandidatesFromPrevAct::prepareQuery()
{
   stringstream sqlStmt;

   sqlStmt.precision(PrintPrecision);    
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

   // get the confirmed candidates on this target in this dx's freq range:
   // TBD: add grid candidate reasons 

   sqlStmt << "SELECT activityId, rfFreq, sigClass, reason, drift, res,"
	   << " UNIX_TIMESTAMP(activityStartTime), dxNumber, signalIdNumber,"
	   << " type, pfa, snr"
	   << " FROM " << CandidateSignalTableName
	   << " WHERE activityId = " << previousActId_
	   << " AND targetId = " << actUnit_->targetId_
	   << " AND rfFreq > " << actUnit_->dxBandLowerFreqLimitMHz_
	   << " AND rfFreq < " << actUnit_->dxBandUpperFreqLimitMHz_
	   << " AND sigClass = '"
	   << SseDxMsg::signalClassToString(CLASS_CAND)
	   << "'"
	   << " AND (reason = '"
	   << SseDxMsg::signalClassReasonToBriefString(CONFIRM)
	   << "'"
	   << " OR reason = '"
	   << SseDxMsg::signalClassReasonToBriefString(RECONFIRM)
	   << "'"
	   << ")";

   /*
     If this is an OFF observation, then only get candidates
     that this dx saw originally.  This avoids a problem where
     adjacent, overlapped dxs try to look for a signal that they
     did not see in the previous ON, and don't see again in the OFF,
     thus keeping it alive as a candidate even when the original dx 
     sees it in the OFF and thus resolves it as RFI.
   */
   if (actUnit_->actOpsBitEnabled(OFF_OBSERVATION))
   {
      sqlStmt << "AND dxNumber = " << actUnit_->getDxNumber();
   }

   sqlStmt << " ORDER by rfFreq ";


   return sqlStmt.str();
}

// Look up the confirmed CwCoherent and Pulse candidates for the
// specified previous activity id that fall within this dx's band.
// Return them in the infoList, which includes all fields needed 
// for follow up requests.

void LookUpCandidatesFromPrevAct::processQueryResults()
{
   const string methodName(getSubclassName() + "processQueryResults()");

   VERBOSE2(getVerboseLevel(), methodName + " for "
	    << actUnit_->getDxName() << endl;);

   int duplicateCount(0);
   processCandidates(infoList_, duplicateCount);

   if (infoList_.size() == 0)
   {
      VERBOSE2(getVerboseLevel(), "Act " << actUnit_->getActivityId() << ":"
	       << " No candidates found for followup of prevactid "
	       << previousActId_ << " that fall in the band of "
	       << actUnit_->getDxName() << endl;);
   }
   else
   {
      SseArchive::SystemLog() 
	 << "Act " << actUnit_->getActivityId() << ":"
	 << " Following up " << infoList_.size()
	 << " candidate(s) of prevactid " 
	 << previousActId_ << " that fall in the band of "
	 << actUnit_->getDxName() 
	 << " (" << duplicateCount << " duplicates were removed)" << endl;
   }   
}



void LookUpCandidatesFromPrevAct::processCandidates(
   FollowUpSignalInfoList &infoList,
   int & duplicateCount)
{
   // Fetch the candidate from each row, convert the values,
   // store them in the info list.
   // Eliminate candidates that are too close in frequency
   // so that that dx does not get confused.
   // Assumes candidates are sorted in freq order.

   // TBD handle duplicate pulses separately from 
   // cw signals

   // This is how far apart the candidates must be 
   // to be considered unique:
   // (TBD pull this out as an activity parameter)

   const double minCwFreqDiffMhz = 0.000100;  

   double prevCwRfFreqMhz = 0.0;
   duplicateCount=0;
   Assert(getResultSet() != 0);
   while (MYSQL_ROW row = mysql_fetch_row(getResultSet()))
   {
      // convert the values
      FollowUpSignalInfo sigInfo;

      // tbd error checking
      // SignalType
      sigInfo.signalType = FollowUpSignalInfo::stringToSignalType(
	 MysqlQuery::getString(row, typeCol, __FILE__, __LINE__));

      // FollowupSignal
      sigInfo.followUpSignal.rfFreq = MysqlQuery::getDouble(
	 row, rfFreqCol, __FILE__, __LINE__);

      sigInfo.followUpSignal.drift = static_cast<float>(MysqlQuery::getDouble(
	 row, driftCol, __FILE__, __LINE__));
      // TBD sig.res

      // FollowUpSignal - original signal Id
      sigInfo.followUpSignal.origSignalId.dxNumber = MysqlQuery::getInt(
	 row, dxNumberCol, __FILE__, __LINE__);

      sigInfo.followUpSignal.origSignalId.activityId = MysqlQuery::getInt(
	 row, activityIdCol, __FILE__, __LINE__);

      sigInfo.followUpSignal.origSignalId.activityStartTime.tv_sec =
	 MysqlQuery::getInt(row, actStartTimeSecsCol,
					  __FILE__, __LINE__);

      sigInfo.followUpSignal.origSignalId.number = MysqlQuery::getInt(
	 row, signalIdNumberCol, __FILE__, __LINE__);

      int activityId = sigInfo.followUpSignal.origSignalId.activityId; 
      int dxNumber = sigInfo.followUpSignal.origSignalId.dxNumber;
      int signalNumber = sigInfo.followUpSignal.origSignalId.number;

      string reason(MysqlQuery::getString(row, reasonCol,
					  __FILE__, __LINE__));

      /*
        pfa and snr might be null if the previous obs was an OFF,
        just use the defaults in that case.
      */
      if (row[pfaCol])
      {
         sigInfo.cfm.pfa = static_cast<float>(
            MysqlQuery::getDouble(row, pfaCol, __FILE__, __LINE__));
      }    
     
      if (row[snrCol])
      {
         sigInfo.cfm.snr = static_cast<float>(
            MysqlQuery::getDouble(row, snrCol, __FILE__, __LINE__));
      }

      VERBOSE2(getVerboseLevel(),"candidate followup: \n"
	       << "reason: " << reason
	       << " actid: " << activityId << " dxnum: " << dxNumber
	       << " signum: " << signalNumber << endl;);

      // only keep the signals that are far enough apart in freq
      // TBD add a separate clause for pulses
      
      if (sigInfo.followUpSignal.rfFreq - prevCwRfFreqMhz >  minCwFreqDiffMhz)
      {
	 VERBOSE2(getVerboseLevel(),"candidate from orig act to be "
		  << "followed up: \n" << sigInfo.followUpSignal << sigInfo.cfm;);
	  
	 // store the signal on the list by value
	 infoList.push_back(sigInfo);
	  
	 prevCwRfFreqMhz = sigInfo.followUpSignal.rfFreq;
      } 
      else
      {
	 duplicateCount++;
	  
	 VERBOSE2(getVerboseLevel(),"followup candidate too close to previous "
		  << "candidate, discarded: \n"
		  << sigInfo.followUpSignal;);
      }
   }
}

// ------------------------------------------------

LookUpCandidatesFromSetiLive::LookUpCandidatesFromSetiLive(
   ActivityUnitImp *activityUnit,
   MYSQL *callerDbConn,
   ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
   ActivityUnitImp::PulseTrainList &pulseTrainList,
   ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList)
   :
   DbQuery(callerDbConn, activityUnit->getActivityId(),
		              activityUnit->getVerboseLevel()),
   actUnit_(activityUnit),
   conn_(callerDbConn),
   cwPowerSigList_(cwPowerSigList),
   pulseTrainList_(pulseTrainList),
   cwCoherentSigList_(cwCoherentSigList)
{
   setNumberOfRequestedCols(nRequestedCols);
   setContext(actUnit_->getDxName());
   setSubclassName("LookUpCandidatesFromSetiLive");
}

LookUpCandidatesFromSetiLive::~LookUpCandidatesFromSetiLive()
{
}

string LookUpCandidatesFromSetiLive::prepareQuery()
{
   stringstream sqlStmt;

   sqlStmt.precision(PrintPrecision);    
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

   // get the confirmed candidates on this target in this dx's freq range:
   // TBD: add grid candidate reasons 

   sqlStmt << "SELECT "
	   << "id, activityId, type, rfFreq, drift, width, power, " 
	   << "pol, sigClass, reason, subchanNumber, containsBadBands, "
	   << "dxNumber, UNIX_TIMESTAMP(activityStartTime), signalIdNumber, "
	   << "origDxNumber, origActivityId, "
	   << "UNIX_TIMESTAMP(origActivityStartTime), "
	   << "origSignalIdNumber, "
	   << "pfa, snr, nSegments, "
	   << "pulsePeriod, numberOfPulses, res "
	   << " FROM " << SetiLiveCandidateSignalTableName
	   << " WHERE activityId = " << actId_
	   << " AND targetId = " << actUnit_->targetId_
	   << " AND rfFreq > " << actUnit_->dxBandLowerFreqLimitMHz_
	   << " AND rfFreq < " << actUnit_->dxBandUpperFreqLimitMHz_
	   << " AND sigClass = '"
	   << SseDxMsg::signalClassToString(CLASS_CAND)
	   << "'"
	   << " AND (reason = '"
	   << SseDxMsg::signalClassReasonToBriefString(CONFIRM)
	   << "'"
	   << " OR reason = '"
	   << SseDxMsg::signalClassReasonToBriefString(RECONFIRM)
	   << "'"
	   << ")";
 
   /*
     If this is an OFF observation, then only get candidates
     that this dx saw originally.  This avoids a problem where
     adjacent, overlapped dxs try to look for a signal that they
     did not see in the previous ON, and don't see again in the OFF,
     thus keeping it alive as a candidate even when the original dx 
     sees it in the OFF and thus resolves it as RFI.
   */
   if (actUnit_->actOpsBitEnabled(OFF_OBSERVATION))
   {
      sqlStmt << "AND dxNumber = " << actUnit_->getDxNumber();
   }

   sqlStmt << " ORDER by rfFreq ";


   return sqlStmt.str();
}

// Look up the confirmed CwCoherent and Pulse candidates for the
// specified previous activity id that fall within this dx's band.
// Return them in the infoList, which includes all fields needed 
// for follow up requests.

void LookUpCandidatesFromSetiLive::processQueryResults()
{
   const string methodName(getSubclassName() + "processQueryResults()");

   VERBOSE2(getVerboseLevel(), methodName + " for "
	    << actUnit_->getDxName() << endl;);

   int duplicateCount(0);
   processCandidates(cwPowerSigList_, pulseTrainList_,
		   cwCoherentSigList_, duplicateCount);

   if ((cwPowerSigList_.size() == 0) && (pulseTrainList_.size() == 0))
   {
      VERBOSE2(getVerboseLevel(), "Act " << actUnit_->getActivityId() << ":"
	       << " No SetiLive candidates found for Activity "
	       << actId_ << ",  " << actUnit_->getDxName() << endl;);
   }
   else
   {
      SseArchive::SystemLog() 
	 << "Act " << actUnit_->getActivityId() << ":"
	 << " Following up " << cwPowerSigList_.size() + pulseTrainList_.size()
	 << " candidate(s) of actid " 
	 << actId_ << " that fall in the band of "
	 << actUnit_->getDxName() 
	 << " (" << duplicateCount << " duplicates were removed)" << endl;
   }   
}



void LookUpCandidatesFromSetiLive::processCandidates(
 ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
    ActivityUnitImp::PulseTrainList &pulseTrainList,
       ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList,
   int & duplicateCount)
{
   // Fetch the candidate from each row, convert the values,
   // store them in the info list.
   // Eliminate candidates that are too close in frequency
   // so that that dx does not get confused.
   // Assumes candidates are sorted in freq order.

   // TBD handle duplicate pulses separately from 
   // cw signals

   // This is how far apart the candidates must be 
   // to be considered unique:
   // (TBD pull this out as an activity parameter)

   string methodName(getSubclassName() + "::processCandidates");
   duplicateCount=0;
   Assert(getResultSet() != 0);
   // Fetch the candidate from each row, convert the values, store them.
   while (MYSQL_ROW row = mysql_fetch_row(getResultSet()))
   {
      try
      {
	 SignalDescription descrip;
	 extractSignalDescription(row, descrip);

	 ConfirmationStats cfmStats;
	 extractConfirmationStats(row, cfmStats);

	 string sigType(MysqlQuery::getString(row, typeCol, 
					      __FILE__, __LINE__));
	  
	 // For now not Coherent Signals
	 if (sigType == CwPowerSigType)
	 {
	    // Use the cwCoherentSignal as the CwPowerSignal for now
	    CwPowerSignal cwPowerSig;
	    cwPowerSig.sig = descrip;
	    cwPowerSigList_.push_back(cwPowerSig);
	    actUnit_->getObsSummaryStats().confirmedCwCandidates++;
	    actUnit_->getCondensedCandidateReport()->addSignal(
			    sigType, descrip, cfmStats);
	      
	    //CwCoherentSignal cwCohSig;
	    //cwCohSig.sig = descrip;
	    //cwCohSig.cfm = cfmStats;
	    //cwCohSig.nSegments = 0;

	    // -- segments --
            // segments not currently used by the dx
	    //     nSegments = MysqlQuery::getInt(
	    //       row, nSegmentsCol));
	    // somday fetch the segments from the database...

	    //cwCoherentSigList.push_back(cwCohSig);

	 }
	 // No pulse Signals either
	 else if (sigType == PulseSigType)
	 {
	    int numberOfPulses = MysqlQuery::getInt(
	       row, numberOfPulsesCol, __FILE__, __LINE__);

	    if (numberOfPulses < 1)
	    {
	       throw SseException( "number of pulses in signal is < 1\n",
				   __FILE__, __LINE__, 
				   SSE_MSG_DBERR, SEVERITY_ERROR);
	    }

	    ActivityUnitImp::PulseTrain *pulseTrain = 
	       new ActivityUnitImp::PulseTrain();
	    pulseTrain->hdr.sig = descrip;
	    pulseTrain->hdr.cfm = cfmStats;
	      
	    extractPulseTrainDescription(row, pulseTrain->hdr.train);
	    pulseTrain->pulseArray = new Pulse[numberOfPulses];

	    // Fetch the pulses
	    unsigned int signalTableId = 
	       static_cast<unsigned int>(MysqlQuery::getInt(
	       row, signalTableIdCol, __FILE__, __LINE__));

	    getPulsesForSignal(conn_,
	       CandidatePulseTrainTableName,
	       signalTableId, pulseTrain->pulseArray,
	       numberOfPulses);

	    pulseTrainList.push_back(pulseTrain);
	    actUnit_->getObsSummaryStats().confirmedPulseCandidates++;
	    actUnit_->getCondensedCandidateReport()->addSignal(
			    sigType, descrip, cfmStats);

	 }
	 else 
	 {
	    AssertMsg(0,"Invalid signal type");
	 }
      }
      catch (SseException &except)
      {
	 SseMessage::log(actUnit_->getDxName(),
                         actUnit_->getActivityId(), 
                         SSE_MSG_EXCEPTION,
                         SEVERITY_ERROR, except.descrip(),
                         except.sourceFilename(), 
                         except.lineNumber());
      }	
      catch (...) 
      { 
	 stringstream strm;
	 strm << " caught unexpected exception in " << methodName 
	      << " for " << actUnit_->getDxName()  << endl;

	 SseMessage::log(MsgSender,
                         actUnit_->getActivityId(), 
                         SSE_MSG_EXCEPTION,
                         SEVERITY_ERROR, strm.str(),
                         __FILE__, __LINE__);

      }

   }
#ifdef obsolete   
   while (MYSQL_ROW row = mysql_fetch_row(getResultSet()))
   {
      // convert the values
      SetiLiveSignalInfo sigInfo;

      // tbd error checking
      // SignalType
      sigInfo.signalType = SetiLiveSignalInfo::stringToSignalType(
	 MysqlQuery::getString(row, typeCol, __FILE__, __LINE__));

      // FollowupSignal
      sigInfo.setiLiveSignal.path.rfFreq = MysqlQuery::getDouble(
	 row, rfFreqCol, __FILE__, __LINE__);

      sigInfo.setiLiveSignal.path.drift = static_cast<float>(MysqlQuery::getDouble(
	 row, driftCol, __FILE__, __LINE__));
   }
#endif
}


void LookUpCandidatesFromSetiLive::extractSignalDescription(
   MYSQL_ROW row, SignalDescription & descrip)
{
   extractSignalPath(row, descrip.path);
   
   descrip.pol = SseMsg::stringToPolarization(
      MysqlQuery::getString(row, polCol, __FILE__, __LINE__));
   
   descrip.sigClass = SseDxMsg::stringToSignalClass(
      MysqlQuery::getString(row, sigClassCol, __FILE__, __LINE__));
   
   descrip.reason = SseDxMsg::briefStringToSignalClassReason(
      MysqlQuery::getString(row, reasonCol, __FILE__, __LINE__));
   
   descrip.subchannelNumber = MysqlQuery::getInt(
      row, subchannelNumberCol, __FILE__, __LINE__);
   
   extractContainsBadBands(row, descrip.containsBadBands);
   
   extractSignalId(row, descrip.signalId);
   
   // Set the "original signal id" fields to be the same values
   // as the current signal.  The dx is going to overwrite
   // the current signal info (dx number etc) as part
   // of its processing. 
   
   descrip.origSignalId = descrip.signalId;

}

void LookUpCandidatesFromSetiLive::extractSignalPath(
   MYSQL_ROW row,
   SignalPath & path)
{  
   path.rfFreq = MysqlQuery::getDouble(
      row, rfFreqCol, __FILE__, __LINE__);

   path.drift = static_cast<float>(MysqlQuery::getDouble(
      row, driftCol, __FILE__, __LINE__));

   path.width = static_cast<float>(MysqlQuery::getDouble(
      row, widthCol, __FILE__, __LINE__));

   path.power = static_cast<float>(MysqlQuery::getDouble(
      row, powerCol, __FILE__, __LINE__));
}

void LookUpCandidatesFromSetiLive::extractContainsBadBands(
   MYSQL_ROW row,
   bool_t & containsBadBands)
{
   string containsBadBandsString(
      MysqlQuery::getString(row, containsBadBandsCol, __FILE__, __LINE__));
   if (containsBadBandsString == "Yes")
   {
      containsBadBands = SSE_TRUE;
   }
   else if (containsBadBandsString == "No")
   {
      containsBadBands = SSE_TRUE;
   }
   else
   {
      throw SseException( "invalid containsBadBands value",
			  __FILE__, __LINE__, 
			  SSE_MSG_DBERR, SEVERITY_ERROR);
   }	      
}

void LookUpCandidatesFromSetiLive::extractSignalId(
   MYSQL_ROW row, SignalId & signalId)
{
   signalId.dxNumber = MysqlQuery::getInt(
	    row, dxNumberCol, __FILE__, __LINE__);
   signalId.activityId = MysqlQuery::getInt(
	    row, activityIdCol, __FILE__, __LINE__);

   // stored act Id should be the same as the current one
   Assert(signalId.activityId == actUnit_->getActivityId());
   
   signalId.activityStartTime.tv_sec = MysqlQuery::getInt(
      row, actStartTimeSecsCol, __FILE__, __LINE__);

   signalId.number = MysqlQuery::getInt(
	    row, signalIdNumberCol, __FILE__, __LINE__);
}

void LookUpCandidatesFromSetiLive::extractConfirmationStats(
   MYSQL_ROW row, ConfirmationStats & cfmStats)
{
   cfmStats.pfa = static_cast<float>(MysqlQuery::getDouble(
      row, pfaCol, __FILE__, __LINE__));
   cfmStats.snr = static_cast<float>(MysqlQuery::getDouble(
      row, snrCol, __FILE__, __LINE__));
}

void LookUpCandidatesFromSetiLive::extractPulseTrainDescription(
   MYSQL_ROW row, PulseTrainDescription & descrip)
{
   descrip.pulsePeriod = MysqlQuery::getDouble(
      row, pulsePeriodCol, __FILE__, __LINE__);
   
   descrip.numberOfPulses =  MysqlQuery::getInt(
      row, numberOfPulsesCol, __FILE__, __LINE__);
   
   descrip.res = SseDxMsg::stringToResolution(
      MysqlQuery::getString(row, resCol, __FILE__, __LINE__));
}


// Fetch pulses corresponding to the signalTableId from the pulseTrainTable and
// put them in the pulseArray.  It's assumed that the pulse array is already
// allocated and of size 'numberOfPulses'.

void LookUpCandidatesFromSetiLive::getPulsesForSignal(
   MYSQL *conn,
   const string &pulseTrainTableName, 
   unsigned int signalTableId, 
   Pulse pulseArray[],
   int numberOfPulses)
{
   string methodName("getPulsesForSignal");

   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);    
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision
 
   sqlStmt << "SELECT "
	   << "rfFreq, power, spectrumNumber, binNumber, pol " 
	   << " FROM " << pulseTrainTableName
	   << " WHERE "
	   << " signalTableId = " << signalTableId
	   << " ";

   enum colIndices
   {
      rfFreqCol, powerCol, spectrumNumberCol, binNumberCol, polCol,
      numCols 
   };
   
   MysqlQuery query(conn);
   query.execute(sqlStmt.str(), numCols, __FILE__, __LINE__);

   // check the number of pulses found
   my_ulonglong nPulsesFound = mysql_num_rows(query.getResultSet()); 
   if (nPulsesFound == 0)
   {
      stringstream errorMsg;
      errorMsg << methodName << " : No pulses found for signalTableId "
	       << signalTableId << endl;
      throw SseException(errorMsg.str(), __FILE__, __LINE__,
			 SSE_MSG_DBERR, SEVERITY_WARNING );
   }
   if (static_cast<int>(nPulsesFound) != numberOfPulses)
   {
      stringstream errorMsg;
      errorMsg << methodName << " : Found " << nPulsesFound 
	       << " for signalTableId " << signalTableId
	       << " but expected " << numberOfPulses << endl;
      throw SseException(errorMsg.str(), __FILE__, __LINE__,
			 SSE_MSG_DBERR, SEVERITY_WARNING );
   }

   // Fetch the pulse description from each row, convert the values, store them.
   // Let caller handle any exceptions
   int index(0);
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      Assert(index < numberOfPulses);
      Pulse & pulse(pulseArray[index++]);

      pulse.rfFreq = MysqlQuery::getDouble(row, rfFreqCol, __FILE__, __LINE__);

      pulse.power = static_cast<float>(
	 MysqlQuery::getDouble(row, powerCol, __FILE__, __LINE__));

      pulse.spectrumNumber = 
	 MysqlQuery::getInt(row, spectrumNumberCol, __FILE__, __LINE__);

      pulse.binNumber = MysqlQuery::getInt(row, binNumberCol, __FILE__, __LINE__);

      pulse.pol = SseMsg::stringToPolarization(
	 MysqlQuery::getString(row, polCol, __FILE__, __LINE__));

   }
      
}

// ------------------------------------------------

LookUpCandidatesFromCounterpartDxs::
LookUpCandidatesFromCounterpartDxs(
      ActivityUnitImp *activityUnit,
      MYSQL *callerDbConn,
      ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
      ActivityUnitImp::PulseTrainList &pulseTrainList,
      ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList)
   :
   DbQuery(callerDbConn, activityUnit->getActivityId(),
	   activityUnit->getVerboseLevel()),
   conn_(callerDbConn),
   actUnit_(activityUnit),
   cwPowerSigList_(cwPowerSigList),
   pulseTrainList_(pulseTrainList),
   cwCoherentSigList_(cwCoherentSigList)
{
   setNumberOfRequestedCols(nRequestedCols);
   setContext(actUnit_->getDxName());
   setSubclassName("LookUpCandidatesFromCounterpartDxs");
}

LookUpCandidatesFromCounterpartDxs::
~LookUpCandidatesFromCounterpartDxs()
{
}



string LookUpCandidatesFromCounterpartDxs::prepareQuery()
{
   // Get the confirmed candidates, i.e.:
   // All candidate pulse signals, and all candidate CW signals that 
   // passed coherent detection

   // TBD: Need to add error handling for the case where
   // there are no dxs on other beams that successfully observed
   // this band (ie, that finished the primary signal detection phase).

   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);    
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision
 
   sqlStmt << "SELECT "
	   << "id, activityId, type, rfFreq, drift, width, power, " 
	   << "pol, sigClass, reason, subchanNumber, containsBadBands, "
	   << "dxNumber, UNIX_TIMESTAMP(activityStartTime), signalIdNumber, "
	   << "origDxNumber, origActivityId, "
	   << "UNIX_TIMESTAMP(origActivityStartTime), "
	   << "origSignalIdNumber, "
	   << "pfa, snr, nSegments, "
	   << "pulsePeriod, numberOfPulses, res "
	   << " FROM " << CandidateSignalTableName
	   << " WHERE "
	   << " activityId = " << actUnit_->getActivityId()
	   << " AND rfFreq > " << actUnit_->dxBandLowerFreqLimitMHz_
	   << " AND rfFreq < " << actUnit_->dxBandUpperFreqLimitMHz_
	   << " AND sigClass = '"
	   << SseDxMsg::signalClassToString(CLASS_CAND)
	   << "'"
	   << " AND dxNumber != " << actUnit_->getDxNumber()
	   << " AND beamNumber != " << actUnit_->beamNumber_
	   << " AND "
	   << " ((type = '" << CwCohSigType << "'"
	   << " AND reason = '"
	   << SseDxMsg::signalClassReasonToBriefString(PASSED_COHERENT_DETECT)
	   << "') "
	   << " OR (type = '" << PulseSigType << "'"
	   << " AND reason = '"
	   << SseDxMsg::signalClassReasonToBriefString(PASSED_POWER_THRESH)
	   << "') "
	   << ")"
	   << " ORDER by rfFreq "
	   << " ";

   return sqlStmt.str();
}

// Get all the candidate signals from dxs on other beams 
// that fall in this dx's band.

void LookUpCandidatesFromCounterpartDxs::processQueryResults()
{
   string methodName(getSubclassName() + "::processQueryResults");

   VERBOSE2(getVerboseLevel(), methodName << " for "
	    << actUnit_->getDxName() << endl;);

   processCandidates(cwPowerSigList_, pulseTrainList_,
		     cwCoherentSigList_);

   int totalCandidates(cwCoherentSigList_.size() + 
		       pulseTrainList_.size());
   if (totalCandidates == 0)
   {
      SseArchive::SystemLog() 
	 <<"Act " << actUnit_->getActivityId() << ":"
	 << " No secondary candidates from counterpart dxs"
	 << " found that fall in the band of "
	 << actUnit_->getDxName() << endl;
   }
   else
   {
      SseArchive::SystemLog()
	 <<"Act " << actUnit_->getActivityId() << ":"
	 << " sending " << totalCandidates
	 << " secondary candidate(s)"
	 << " from counterpart dxs that fall in the band of "
	 << actUnit_->getDxName() << endl;
   }

}
       
void LookUpCandidatesFromCounterpartDxs::processCandidates(
   ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
   ActivityUnitImp::PulseTrainList &pulseTrainList,
   ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList)
{
   string methodName(getSubclassName() + "::processCandidates");
 
   // Fetch the candidate from each row, convert the values, store them.
   while (MYSQL_ROW row = mysql_fetch_row(getResultSet()))
   {
      try
      {
	 SignalDescription descrip;
	 extractSignalDescription(row, descrip);

	 ConfirmationStats cfmStats;
	 extractConfirmationStats(row, cfmStats);

	 string sigType(MysqlQuery::getString(row, typeCol, 
					      __FILE__, __LINE__));
	  
	 // Cw Coherent Signal fields only
	 if (sigType == CwCohSigType)
	 {
	    // Use the cwCoherentSignal as the CwPowerSignal for now
	    CwPowerSignal cwPowerSig;
	    cwPowerSig.sig = descrip;
	    cwPowerSigList.push_back(cwPowerSig);
	      
	    CwCoherentSignal cwCohSig;
	    cwCohSig.sig = descrip;
	    cwCohSig.cfm = cfmStats;
	    cwCohSig.nSegments = 0;

	    // -- segments --
            // segments not currently used by the dx
	    //     nSegments = MysqlQuery::getInt(
	    //       row, nSegmentsCol));
	    // somday fetch the segments from the database...

	    cwCoherentSigList.push_back(cwCohSig);

	 }
	 else if (sigType == PulseSigType)
	 {
	    int numberOfPulses = MysqlQuery::getInt(
	       row, numberOfPulsesCol, __FILE__, __LINE__);

	    if (numberOfPulses < 1)
	    {
	       throw SseException( "number of pulses in signal is < 1\n",
				   __FILE__, __LINE__, 
				   SSE_MSG_DBERR, SEVERITY_ERROR);
	    }

	    ActivityUnitImp::PulseTrain *pulseTrain = 
	       new ActivityUnitImp::PulseTrain();
	    pulseTrain->hdr.sig = descrip;
	    pulseTrain->hdr.cfm = cfmStats;
	      
	    extractPulseTrainDescription(row, pulseTrain->hdr.train);
	    pulseTrain->pulseArray = new Pulse[numberOfPulses];

	    // Fetch the pulses
	    unsigned int signalTableId = 
	       static_cast<unsigned int>(MysqlQuery::getInt(
	       row, signalTableIdCol, __FILE__, __LINE__));

	    getPulsesForSignal(conn_,
	       CandidatePulseTrainTableName,
	       signalTableId, pulseTrain->pulseArray,
	       numberOfPulses);

	    pulseTrainList.push_back(pulseTrain);

	 }
	 else 
	 {
	    AssertMsg(0,"Invalid signal type");
	 }
      }
      catch (SseException &except)
      {
	 SseMessage::log(actUnit_->getDxName(),
                         actUnit_->getActivityId(), 
                         SSE_MSG_EXCEPTION,
                         SEVERITY_ERROR, except.descrip(),
                         except.sourceFilename(), 
                         except.lineNumber());
      }	
      catch (...) 
      { 
	 stringstream strm;
	 strm << " caught unexpected exception in " << methodName 
	      << " for " << actUnit_->getDxName()  << endl;

	 SseMessage::log(MsgSender,
                         actUnit_->getActivityId(), 
                         SSE_MSG_EXCEPTION,
                         SEVERITY_ERROR, strm.str(),
                         __FILE__, __LINE__);

      }

   }
  
}

void LookUpCandidatesFromCounterpartDxs::extractSignalDescription(
   MYSQL_ROW row, SignalDescription & descrip)
{
   extractSignalPath(row, descrip.path);
   
   descrip.pol = SseMsg::stringToPolarization(
      MysqlQuery::getString(row, polCol, __FILE__, __LINE__));
   
   descrip.sigClass = SseDxMsg::stringToSignalClass(
      MysqlQuery::getString(row, sigClassCol, __FILE__, __LINE__));
   
   descrip.reason = SseDxMsg::briefStringToSignalClassReason(
      MysqlQuery::getString(row, reasonCol, __FILE__, __LINE__));
   
   descrip.subchannelNumber = MysqlQuery::getInt(
      row, subchannelNumberCol, __FILE__, __LINE__);
   
   extractContainsBadBands(row, descrip.containsBadBands);
   
   extractSignalId(row, descrip.signalId);
   
   // Set the "original signal id" fields to be the same values
   // as the current signal.  The dx is going to overwrite
   // the current signal info (dx number etc) as part
   // of its processing. 
   
   descrip.origSignalId = descrip.signalId;

}

void LookUpCandidatesFromCounterpartDxs::extractSignalPath(
   MYSQL_ROW row,
   SignalPath & path)
{  
   path.rfFreq = MysqlQuery::getDouble(
      row, rfFreqCol, __FILE__, __LINE__);

   path.drift = static_cast<float>(MysqlQuery::getDouble(
      row, driftCol, __FILE__, __LINE__));

   path.width = static_cast<float>(MysqlQuery::getDouble(
      row, widthCol, __FILE__, __LINE__));

   path.power = static_cast<float>(MysqlQuery::getDouble(
      row, powerCol, __FILE__, __LINE__));
}

void LookUpCandidatesFromCounterpartDxs::extractContainsBadBands(
   MYSQL_ROW row,
   bool_t & containsBadBands)
{
   string containsBadBandsString(
      MysqlQuery::getString(row, containsBadBandsCol, __FILE__, __LINE__));
   if (containsBadBandsString == "Yes")
   {
      containsBadBands = SSE_TRUE;
   }
   else if (containsBadBandsString == "No")
   {
      containsBadBands = SSE_TRUE;
   }
   else
   {
      throw SseException( "invalid containsBadBands value",
			  __FILE__, __LINE__, 
			  SSE_MSG_DBERR, SEVERITY_ERROR);
   }	      
}

void LookUpCandidatesFromCounterpartDxs::extractSignalId(
   MYSQL_ROW row, SignalId & signalId)
{
   signalId.dxNumber = MysqlQuery::getInt(
	    row, dxNumberCol, __FILE__, __LINE__);
   signalId.activityId = MysqlQuery::getInt(
	    row, activityIdCol, __FILE__, __LINE__);

   // stored act Id should be the same as the current one
   Assert(signalId.activityId == actUnit_->getActivityId());
   
   signalId.activityStartTime.tv_sec = MysqlQuery::getInt(
      row, actStartTimeSecsCol, __FILE__, __LINE__);

   signalId.number = MysqlQuery::getInt(
	    row, signalIdNumberCol, __FILE__, __LINE__);
}

void LookUpCandidatesFromCounterpartDxs::extractConfirmationStats(
   MYSQL_ROW row, ConfirmationStats & cfmStats)
{
   cfmStats.pfa = static_cast<float>(MysqlQuery::getDouble(
      row, pfaCol, __FILE__, __LINE__));
   cfmStats.snr = static_cast<float>(MysqlQuery::getDouble(
      row, snrCol, __FILE__, __LINE__));
}

void LookUpCandidatesFromCounterpartDxs::extractPulseTrainDescription(
   MYSQL_ROW row, PulseTrainDescription & descrip)
{
   descrip.pulsePeriod = MysqlQuery::getDouble(
      row, pulsePeriodCol, __FILE__, __LINE__);
   
   descrip.numberOfPulses =  MysqlQuery::getInt(
      row, numberOfPulsesCol, __FILE__, __LINE__);
   
   descrip.res = SseDxMsg::stringToResolution(
      MysqlQuery::getString(row, resCol, __FILE__, __LINE__));
}


// Fetch pulses corresponding to the signalTableId from the pulseTrainTable and
// put them in the pulseArray.  It's assumed that the pulse array is already
// allocated and of size 'numberOfPulses'.

void LookUpCandidatesFromCounterpartDxs::getPulsesForSignal(
   MYSQL *conn,
   const string &pulseTrainTableName, 
   unsigned int signalTableId, 
   Pulse pulseArray[],
   int numberOfPulses)
{
   string methodName("getPulsesForSignal");

   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);    
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision
 
   sqlStmt << "SELECT "
	   << "rfFreq, power, spectrumNumber, binNumber, pol " 
	   << " FROM " << pulseTrainTableName
	   << " WHERE "
	   << " signalTableId = " << signalTableId
	   << " ";

   enum colIndices
   {
      rfFreqCol, powerCol, spectrumNumberCol, binNumberCol, polCol,
      numCols 
   };
   
   MysqlQuery query(conn);
   query.execute(sqlStmt.str(), numCols, __FILE__, __LINE__);

   // check the number of pulses found
   my_ulonglong nPulsesFound = mysql_num_rows(query.getResultSet()); 
   if (nPulsesFound == 0)
   {
      stringstream errorMsg;
      errorMsg << methodName << " : No pulses found for signalTableId "
	       << signalTableId << endl;
      throw SseException(errorMsg.str(), __FILE__, __LINE__,
			 SSE_MSG_DBERR, SEVERITY_WARNING );
   }
   if (static_cast<int>(nPulsesFound) != numberOfPulses)
   {
      stringstream errorMsg;
      errorMsg << methodName << " : Found " << nPulsesFound 
	       << " for signalTableId " << signalTableId
	       << " but expected " << numberOfPulses << endl;
      throw SseException(errorMsg.str(), __FILE__, __LINE__,
			 SSE_MSG_DBERR, SEVERITY_WARNING );
   }

   // Fetch the pulse description from each row, convert the values, store them.
   // Let caller handle any exceptions
   int index(0);
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      Assert(index < numberOfPulses);
      Pulse & pulse(pulseArray[index++]);

      pulse.rfFreq = MysqlQuery::getDouble(row, rfFreqCol, __FILE__, __LINE__);

      pulse.power = static_cast<float>(
	 MysqlQuery::getDouble(row, powerCol, __FILE__, __LINE__));

      pulse.spectrumNumber = 
	 MysqlQuery::getInt(row, spectrumNumberCol, __FILE__, __LINE__);

      pulse.binNumber = MysqlQuery::getInt(row, binNumberCol, __FILE__, __LINE__);

      pulse.pol = SseMsg::stringToPolarization(
	 MysqlQuery::getString(row, polCol, __FILE__, __LINE__));

   }
      
}

// ------------------------------------------------

PrepareFakeSecondaryCandidatesToForceArchiving::
PrepareFakeSecondaryCandidatesToForceArchiving(
   ActivityUnitImp *actUnit,
   ActivityUnitImp::CwPowerSignalList &cwPowerSigList,
   ActivityUnitImp::PulseTrainList &pulseTrainList,
   ActivityUnitImp::CwCoherentSignalList &cwCoherentSigList)
{
   // create as many fake candidates as needed to force the dx to 
   // archive a given bandwidth around the dx center tune sky freq

   // enough for max doppler shift of cosmos1 + some fudge
   double desiredTotalArchiveBwKhz = 120.0;  

   //ie, ~ 16 subchannel's worth (a bit less for some overlap)
   // 16 subchannels hold 10.8 Khz
   double bwArchivePerSignalKhz = 9.0;  

   int nFakeSignals = static_cast<int>(desiredTotalArchiveBwKhz /
				       bwArchivePerSignalKhz) + 1;

   double startFreqOffsetMhz = (desiredTotalArchiveBwKhz / 2) /
      SseAstro::KhzPerMhz;
   double skyFreqMhz = actUnit->dxProxy_->getDxSkyFreq() - 
      startFreqOffsetMhz;
   double freqStepMhz = bwArchivePerSignalKhz / SseAstro::KhzPerMhz;

   // make it obvious that it's fake (larger than any regular signal num)
   int signalNumber = 50000;  
   for (int i = 0; i < nFakeSignals; ++i)
   {
      SignalDescription descrip;

      descrip.path.rfFreq = skyFreqMhz;

      // just set some plausible values for the rest of the fields
      descrip.path.drift = 0.01; 
      descrip.path.width = 0.1;  
      descrip.path.power = 100;

      descrip.signalId.number = signalNumber++;
      descrip.signalId.activityId = actUnit->getActivityId();
      descrip.signalId.dxNumber = actUnit->getDxNumber();
      descrip.signalId.activityStartTime = 
	 actUnit->obsActivity_->getStartTimeAsNssDate();

      // just set some plausible values for the rest of the fields
      descrip.pol = POL_BOTH;
      descrip.sigClass = CLASS_CAND;
      descrip.reason = CLASS_REASON_UNINIT;
      descrip.subchannelNumber = -1;
      descrip.containsBadBands = SSE_FALSE;

      // Set the "original signal id" fields to be the same values
      // as the current signal.  The dx is going to overwrite
      // the current signal info (dx number etc) as part
      // of its processing. 

      descrip.origSignalId = descrip.signalId;
      
      ConfirmationStats cfmStats;
      cfmStats.pfa = -10;
      cfmStats.snr = 0.5;

      // Use the cwCoherentSignal as the CwPowerSignal for now
      CwPowerSignal cwPowerSig;
      cwPowerSig.sig = descrip;
      cwPowerSigList.push_back(cwPowerSig);
      
      CwCoherentSignal cwCohSig;
      cwCohSig.sig = descrip;
      cwCohSig.cfm = cfmStats;
      cwCohSig.nSegments = 0;
      // -- segments not currently used by the dx --
      
      cwCoherentSigList.push_back(cwCohSig);

      // debug
      //cout << "\nfake signal sent\n" << cwCohSig << endl;

      skyFreqMhz += freqStepMhz;
   }
}
   
PrepareFakeSecondaryCandidatesToForceArchiving::
~PrepareFakeSecondaryCandidatesToForceArchiving()
{}

// ------------------------------------------------

ResolveCandidatesBasedOnSecondaryProcessingResults::
ResolveCandidatesBasedOnSecondaryProcessingResults(ActivityUnitImp *activityUnit,
                                                   MYSQL *callerDbConn)
   :
   DbQuery(callerDbConn, activityUnit->getActivityId(), 
	   activityUnit->getVerboseLevel()),
   conn_(callerDbConn),
   actUnit_(activityUnit),
   usingNulls_(activityUnit->getObsAct()->getActParameters().useMultiTargetNulls())
{
   setNumberOfRequestedCols(nRequestedCols);
   setContext(actUnit_->getDxName());
   setSubclassName("ResolveCandidatesBasedOnSecondaryProcessingResults");

   double nullDepthDb = activityUnit->getObsAct()->getActParameters().getNullDepthDb();
   nullDepthLinear_ = SseUtil::dbToLinearRatio(nullDepthDb);
}

ResolveCandidatesBasedOnSecondaryProcessingResults::
~ResolveCandidatesBasedOnSecondaryProcessingResults()
{
}



// Look for counterpart dx secondary results for this dx's candidates,
// (previously stored).  These can be found by looking for signals
// whose dxNumber does NOT match this dx, but whose origDxNumber
// does.

string ResolveCandidatesBasedOnSecondaryProcessingResults::prepareQuery()
{
   stringstream sqlStmt;
   sqlStmt.precision(PrintPrecision);    
   sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision
   
   sqlStmt << "SELECT "
	   << "dxNumber, type, rfFreq, drift, sigClass, "
	   << "reason, signalIdNumber, origSignalIdNumber, "
           << "pfa, snr "
	   << " FROM " << CandidateSignalTableName
	   << " WHERE "
	   << " activityId = " << actUnit_->getActivityId()
	   << " AND dxNumber != " << actUnit_->getDxNumber()
	   << " AND origDxNumber = " << actUnit_->getDxNumber()
	   << " AND ( reason = '"
	   << SseDxMsg::signalClassReasonToBriefString(SECONDARY_FOUND_SIGNAL)
	   << "' OR reason = '" 
	   << SseDxMsg::signalClassReasonToBriefString(SECONDARY_NO_SIGNAL_FOUND)
	   << "') ";

   return sqlStmt.str();
}

// Update the primary candidate sig class & reason based on the secondary
// candidate results.
void ResolveCandidatesBasedOnSecondaryProcessingResults::
processQueryResults()
{
   const string methodName(getSubclassName() + "::processQueryResults()");

   // Get the counterpart dx secondary results for this dx's candidates
   CandSigMultiMap counterpartSecondaryCandSigMultiMap;

   fetchCounterpartCandidates(counterpartSecondaryCandSigMultiMap);

   // Go through the candidate list for this dx.
   // For each candidate, look for counterparts who saw the same signal.
   // If at least one did, then mark it as RFI, else call it confirmed.
   // Update the sig class & reason in the database and update 
   // the appropriate log files.

   for(ActivityUnitImp::CandSigInfoList::iterator candIt = 
	  actUnit_->candSigInfoList_.begin(); 
       candIt != actUnit_->candSigInfoList_.end(); ++candIt)
   {
      ActivityUnitImp::CandSigInfo & candSigInfo = *candIt;

      if (candSigInfo.descrip.sigClass == CLASS_CAND)
      {
	 // find all counterpart signal id's that match this one
	 int signalNumber = candSigInfo.descrip.signalId.number;
	 bool signalWasSeen(false);
	 if (! findCounterpartResultsForSignal(
	    signalNumber, 
            candSigInfo.cfm.snr,
	    counterpartSecondaryCandSigMultiMap,
	    signalWasSeen))
	 {
	    // TBD error handling, no counterpart signals found

	    stringstream strm;
	    strm << methodName << ": " 
		 << "no secondary (counterpart) signals available for "
		 << " signal number " << signalNumber
		 << " for " << actUnit_->getDxName() << endl;
	    SseMessage::log(MsgSender, actUnit_->getActivityId(),
                            SSE_MSG_NO_SEC_SIG, SEVERITY_WARNING,
                            strm.str(), __FILE__, __LINE__);
	    continue;
	 }

	 // update signal class and reason
	 SignalClass sigClass(CLASS_UNINIT);
	 SignalClassReason reason(CLASS_REASON_UNINIT);

	 // TBD revisit for ON & OFF observations
	 if (signalWasSeen)
	 {
	    sigClass = CLASS_RFI;
	    reason = SEEN_MULTIPLE_BEAMS;
	 }
	 else
	 {
	    sigClass = CLASS_CAND;
	    if (actUnit_->actOpsBitEnabled(ON_OBSERVATION))
	    {
	       reason = RECONFIRM;
	    } 
	    else
	    {
	       reason = CONFIRM;
	    }
	    countSignalAsConfirmed(candSigInfo.sigType, signalNumber);
	 }

	 updateCandidateSignalClassAndReasonInDb(
	    candSigInfo.dbTableKeyId, sigClass, reason);
	
	 candSigInfo.descrip.sigClass = sigClass;
	 candSigInfo.descrip.reason = reason;

      }

      // update signal log
      actUnit_->getCondensedCandidateReport()->addSignal(
	 candSigInfo.sigType,
	 candSigInfo.descrip,
	 candSigInfo.cfm);

      // send archive request for primary candidate
      actUnit_->determineArchiveRequest(actUnit_->dxProxy_, 
					candSigInfo.descrip);

   }

   actUnit_->sendArchiveRequestsForSecondaryCandidates();

}

void ResolveCandidatesBasedOnSecondaryProcessingResults::
countSignalAsConfirmed(const string &sigType, int signalNumber)
{
   const string methodName(getSubclassName() + "countSignalAsConfirmed");

   // count the signal as confirmed
   if (sigType == PulseSigType)
   {
      actUnit_->getObsSummaryStats().confirmedPulseCandidates++;
   }
   else if (sigType == CwCohSigType)
   {
      actUnit_->getObsSummaryStats().confirmedCwCandidates++;
   }
   else
   {
      stringstream strm;
      
      strm << methodName << " unexpected signal type: "
	   << sigType 
	   << " for signal number " << signalNumber
	   << " for " << actUnit_->getDxName() << endl;
      
      SseMessage::log(MsgSender, actUnit_->getActivityId(),
                      SSE_MSG_DBERR, SEVERITY_ERROR,
                      strm.str(), __FILE__, __LINE__);
   }
}

/*
  Look up counterpart signals in the map corresponding to
  the signal number.
  Set signalWasSeen if any other dx saw the signal, 
  unless its SNR is reduced by the expected amount of nulling.
  Function value is true if counterpart signals are found,
  otherwise false.
*/

bool ResolveCandidatesBasedOnSecondaryProcessingResults::
findCounterpartResultsForSignal(
   int signalNumber,
   double signalSnr,
   CandSigMultiMap & candSigMultiMap,
   bool & signalWasSeen)
{
   const string methodName(getSubclassName() + "findCounterpartResultsForSignal");

   signalWasSeen = false;
   bool counterpartsFound(false);
   for (CandSigMultiMap::iterator counterpartIt = 
	   candSigMultiMap.lower_bound(signalNumber);
	counterpartIt != 
	   candSigMultiMap.upper_bound(signalNumber);
	++counterpartIt)
   {
      counterpartsFound = true;

      SignalDescription & counterpartDescrip = (counterpartIt->second).descrip;
      
      // TBD revisit this for ON/OFF observations
      if (counterpartDescrip.reason == SECONDARY_FOUND_SIGNAL)
      {
         /*
           Signal was seen, however, if nulls are being used in counterpart 
           beams, then the signal is not considered RFI if its strength was 
           reduced in a manner consistent with the expected null effectiveness.
         */
         if (usingNulls_)
         {
            double counterpartSnr = (counterpartIt->second).cfm.snr;
            double snrRatio = signalSnr / counterpartSnr;

            string result;
	    // for a pfa of -70, the average snr is .400
	    if (signalSnr <= .400 && signalSnr >= .150)
	    {
		    // for a pfa of -20, the average snr is .100
		    // above -20 is had to tell from the noise.
	       if (counterpartSnr > .100 )
            	{
               	signalWasSeen = true;
                result = "Signal is RFI, too strong.";
            	}
            	else
            	{
               	result = "Signal is potential candidate.";
            	}
	    }
            else if (snrRatio < nullDepthLinear_)
            {
               signalWasSeen = true;
               result = "Signal is RFI, too strong.";
            }
            else
            {
               result = "Signal is potential candidate.";
            }

            SseArchive::SystemLog()
               << "Act " << actUnit_->getActivityId() << ":"
               << " Multibeam null compare:"
               << " dx" << actUnit_->getDxNumber() 
               << " sig# " << signalNumber
               << " vs. null dx" << counterpartDescrip.signalId.dxNumber
               << " sig# " << counterpartDescrip.signalId.number
               << " snrRatio: " << snrRatio 
               << " (" << SseUtil::linearRatioToDb(snrRatio) << " dB). "
               << result << endl;

            if (signalWasSeen)
            {
               break;
            }
         }
         else 
         {
            // No nulls in use, signal was seen by at least one counterpart,
            // so no need to look at any more results

            signalWasSeen = true;
            break;
         }
      }
      else if (counterpartDescrip.reason == SECONDARY_NO_SIGNAL_FOUND)
      {
	 // do nothing here, another dx may still have seen it
      }
      else
      {  
         // invalid signal class reason
	 stringstream strm;
	 
	 strm << methodName << " unexpected signal class reason: "
	      << SseDxMsg::signalClassReasonToBriefString(counterpartDescrip.reason)
	      << " for signal number " << signalNumber
	      << " for " << actUnit_->getDxName() << endl;
	 
	 SseMessage::log(MsgSender, actUnit_->getActivityId(),
                         SSE_MSG_BAD_SIG_CLASS, SEVERITY_ERROR,
                         strm.str(), __FILE__, __LINE__);
      }
   }

   return (counterpartsFound);
}



void ResolveCandidatesBasedOnSecondaryProcessingResults::
fetchCounterpartCandidates(CandSigMultiMap & candSigMultiMap)
{
   const string methodName(getSubclassName() + "::fetchCounterpartCandidates()");

   // Fetch the candidate from each row, convert the values, store them.
   while (MYSQL_ROW row = mysql_fetch_row(getResultSet()))
   {
      try
      {
         CandSig candSig;
	 extractCandSig(row, candSig);

	 candSigMultiMap.insert(
	    make_pair(candSig.descrip.origSignalId.number,candSig));
      }
      catch (SseException &except)
      {
	 stringstream strm;
	 strm << methodName << " " << except 
	      << " for " << actUnit_->getDxName() << endl;
	 SseMessage::log(MsgSender,
                         actUnit_->getActivityId(), SSE_MSG_EXCEPTION,
                         SEVERITY_ERROR, strm.str(),
                         __FILE__, __LINE__);
      }	
   }
}

void ResolveCandidatesBasedOnSecondaryProcessingResults::
extractCandSig(MYSQL_ROW row, CandSig & candSig)
{
   //string sigType(Mysql::getString(row, typeCol, __FILE__, __LINE__));

   candSig.descrip.path.rfFreq = MysqlQuery::getDouble(
      row, rfFreqCol, __FILE__, __LINE__);
   
   candSig.descrip.path.drift = static_cast<float>(MysqlQuery::getDouble(
      row, driftCol, __FILE__, __LINE__));

   candSig.descrip.sigClass = SseDxMsg::stringToSignalClass(
      MysqlQuery::getString(row, sigClassCol, __FILE__, __LINE__));

   candSig.descrip.reason = SseDxMsg::briefStringToSignalClassReason(
      MysqlQuery::getString(row, reasonCol, __FILE__, __LINE__));

   candSig.descrip.signalId.dxNumber = MysqlQuery::getInt(
      row, dxNumberCol, __FILE__, __LINE__);

   candSig.descrip.signalId.number = MysqlQuery::getInt(
      row, signalIdNumberCol, __FILE__, __LINE__);

   candSig.descrip.origSignalId.number = MysqlQuery::getInt(
      row, origSignalIdNumberCol, __FILE__, __LINE__);

   candSig.cfm.pfa = static_cast<float>(MysqlQuery::getDouble(
      row, pfaCol, __FILE__, __LINE__));

   candSig.cfm.snr = static_cast<float>(MysqlQuery::getDouble(
      row, snrCol, __FILE__, __LINE__));

}

void ResolveCandidatesBasedOnSecondaryProcessingResults::
updateCandidateSignalClassAndReasonInDb(
   const DbTableKeyId dbTableKeyId, 
   const SignalClass sigClass,
   const SignalClassReason reason)
{
   const string methodName(getSubclassName() +
			   "updateCandidateSignalClassAndReasonInDb");

   stringstream sqlStmt;
   sqlStmt << "update " << CandidateSignalTableName << " set"
	   << " sigClass = '" 
	   << SseDxMsg::signalClassToString(sigClass)
	   << "', reason = '" 
	   << SseDxMsg::signalClassReasonToBriefString(reason)
	   << "' where id = " << dbTableKeyId
	   << " ";
   
   actUnit_->submitDbQueryWithLoggingOnError(conn_, sqlStmt.str(),
					     methodName, __LINE__);

}

// ------------------------------------------------

void ActivityUnitImp::submitDbQueryWithLoggingOnError(
   MYSQL *callerDbConn,
   const string &sqlStmt,
   const string &callingMethodName,
   int lineNumber)
{
   if (mysql_query(callerDbConn, sqlStmt.c_str()) != 0)
   {	
      stringstream strm;
      strm << callingMethodName 
	   << " submitDbQuery: MySQL error: " 
	   << mysql_error(callerDbConn)  << " "
           << sqlStmt.c_str() << endl;
      
      SseMessage::log(MsgSender,
                      getActivityId(), SSE_MSG_DBERR,
                      SEVERITY_WARNING, strm.str(),
                      __FILE__, lineNumber);
   }
}

void ActivityUnitImp::submitDbQueryWithThrowOnError(
   MYSQL *conn,
   const string &sqlStmt,
   const string &callingMethodName,
   int lineNumber)
{
   if (mysql_query(conn, sqlStmt.c_str()) != 0)
   {	
      stringstream strm;
      strm << callingMethodName 
	   << " submitDbQuery: MySQL error: " 
	   << mysql_error(conn)  << endl;
      
      throw SseException(strm.str(), __FILE__, lineNumber,
			 SSE_MSG_DBERR, SEVERITY_ERROR);
   }
}

// ------------------------------------------------

#ifdef FIND_TEST_SIGNAL

// Checks to see if the signal matches the selected TestSignal parameters.
// If yes, then sets the SignalDescription classification & reason
// and returns true. 

static bool matchCwPowerTestSignal(
   CwPowerSignal &cwp,
   const TestSigParameters &testSigParameters,
   double ifcSkyFreqMhz)
{
   int nTestsRun = 0;
   int nTestsPassed = 0;

   // Check Frequency
   if (testSigParameters.getCheckFreq())
   {
      // TBD should this be derived from the rf sky freq?
      double testSigSkyFreqMhz = testSigParameters.getFrequency() +
	 ifcSkyFreqMhz;

      nTestsRun++;

      // Check reported sky freq against expected sky freq
      if (fabs(cwp.sig.path.rfFreq - testSigSkyFreqMhz) < 
	  testSigParameters.getFreqTol())
      {
	 nTestsPassed++;
      }
   }

   // Check drift
   if (testSigParameters.getCheckDrift())
   {
      nTestsRun++;
      if (fabs(cwp.sig.path.drift - testSigParameters.getDriftRate()) <
	  testSigParameters.getDriftTol())
      {
	 nTestsPassed++;
      }
   }


/**** CW power has no SNR.  remove this?  TBD 
      // Check SNR
      if (testSigParameters.checkSnr)
      {
      nTestsRun++;
	
      if (fabs(cwp.cfm.snr - testSigParameters.snr) <
      testSigParameters.snrTol)
      {
      nTestsPassed++;
      }
      }
******/

   // Check width
   if (testSigParameters.getCheckWidth())
   {
      nTestsRun++;
      if (fabs(cwp.sig.path.width - testSigParameters.getWidth()) <
	  testSigParameters.getWidthTol())
      {
	 nTestsPassed++;
      }
   }

   // see if all the tests were passed

   if ((nTestsRun > 0) && (nTestsRun == nTestsPassed))
   {
      // identified test signal
      cwp.sig.sigClass = CLASS_TEST;
      cwp.sig.reason = TEST_SIGNAL_MATCH;
      return true;
   }
   else
   {
      return false;
   }
}

#endif

// Update the ObsSummaryStats Signal counts for zero drift and
// recent rfi matches and unknown signals

void ActivityUnitImp::updateObsSummarySignalCounts(SignalClassReason reason){

   if ( reason == ZERO_DRIFT)
      getObsSummaryStats().zeroDriftSignals++;
   if ( reason == RECENT_RFI_MATCH)
      getObsSummaryStats().recentRfiDatabaseMatches++;
   if ( reason == TOO_MANY_CANDIDATES)
      getObsSummaryStats().unknownSignals++;
}
//              subchannel, which is based on the maximum bandwidth of the
//              DX
//
int ActivityUnitImp::getSubchannel(double freq)
{
        int subchannel;
	   double subchannelWidthMhz = dxProxy_->getIntrinsics().hzPerSubchannel/SseAstro::HzPerMhz;
	   double lowFreq = dxProxy_->getDxSkyFreq() - 
	   (subchannelWidthMhz + dxProxy_->getBandwidthInMHz())/2.0;

        subchannel = static_cast<int32_t> ((freq - lowFreq)
                                        / subchannelWidthMhz);
        return (subchannel);
}

