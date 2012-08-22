/*******************************************************************************

 File:    ObsActStrategy.cpp
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

// Strategy for running observation activities

#include "ObsActStrategy.h"
#include "Activity.h"
#include "ActivityException.h"
#include "ActivityWrappers.h"
#include "ActParameters.h"
#include "Assert.h"
#include "AtaInformation.h"
#include "ChannelizerList.h"
#include "ChannelizerParameters.h"
#include "ChannelizerProxy.h"
#include "ComponentControlList.h"
#include "ComponentControlProxy.h"
#include "DbParameters.h"
#include "DebugLog.h"
#include "ExpectedNssComponentsTree.h"
#include "Followup.h"
#include "IfcProxy.h"
#include "MinMaxBandwidth.h"
#include "MinMaxDxSkyFreqMhz.h"
#include "MsgSender.h"
#include "NssComponentTree.h"
#include "NssParameters.h"
#include "OrderedTargets.h"
#include "DxParameters.h"
#include "DxProxy.h"
#include "PermRfiMaskFilename.h"
#include "Scheduler.h"
#include "SchedulerParameters.h"
#include "SignalMask.h"
#include "Site.h"
#include "SseArchive.h"
#include "SseAstro.h"
#include "SseMessage.h"
#include "SseUtil.h"
#include "TargetPosition.h"
#include "TscopeParameters.h"
#include "TscopeProxy.h"
#include "TuneDxs.h"
#include "TuneZxs.h"
#include <algorithm>
#include <memory>
#include <sstream>
#include <iostream>

using namespace std;
static const double AssumedCommensalCalFreqMhz = 1420;

class ActivityData 
{
public:

   enum ActivityStatus {
      ACTIVITY_NOT_STARTED,
      ACTIVITY_DATA_COLLECTION_COMPLETE,
   };

   ActivityData(enum ActivityStatus status)
      : status_(status)
   {}

   ~ActivityData()
   {}
   
   enum ActivityStatus status_;
};

ObsActStrategy::ObsActStrategy(Scheduler *scheduler,
                               Site *site, 
                               const NssParameters &nssParameters,
                               int verboseLevel):
   ActivityStrategy(scheduler, site, nssParameters, verboseLevel),
   nssParameters_(nssParameters),
   tuneDxs_(0),
   followup_(0),
   scheduler_(scheduler),
   followupEnabled_(nssParameters_.sched_->followupEnabled()),
   followupModeIsAuto_(nssParameters_.sched_->followupModeIsAuto()),
   orderedTargets_(0),
   commensalCalTimer_("commensal cal"),
   commensalCalPending_(false),
   rotatePrimaryTargetIdsTimer_("rotate primary target ids"),
   rotatePrimaryTargetIdsPending_(false)
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::ObsActStrategy" << endl;);

   double beginFreqMhz(nssParameters_.sched_->getBeginObsFreqMhz());
   double endFreqMhz(nssParameters_.sched_->getEndObsFreqMhz());
   if (beginFreqMhz > endFreqMhz)
   {
    throw SseException(
         "scheduler parameter error: beginfreq > endfreq\n",
         __FILE__, __LINE__);
   }

   tuneDxs_ = getTuneDxs(getVerboseLevel());
   tuneZxs_ = getTuneZxs(getVerboseLevel());
   followup_ = Followup::instance();
}

ObsActStrategy::~ObsActStrategy()
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::~()\n" );
   delete tuneDxs_;
   delete tuneZxs_;
   delete orderedTargets_;
}

void ObsActStrategy::setVerboseLevelInternalHook(int verboseLevel)
{
   Assert(tuneDxs_);
   tuneDxs_->setVerboseLevel(verboseLevel);
}

void ObsActStrategy::configureCommensalCalTimer()
{
   // prepare commensal cal catalog
   const string phaseCalCatalog("calphase");
   
   calTargets_.loadTargetsFromDb(nssParameters_.db_->getDb(), phaseCalCatalog);
   calTargets_.computeFluxAtFreq(AssumedCommensalCalFreqMhz);
   
   const double SecsPerMin(60);
   int recalPeriodSecs(nssParameters_.sched_->getCommensalCalIntervalMinutes()
                       * SecsPerMin);
   
   commensalCalTimer_.setRepeatIntervalSecs(recalPeriodSecs);
   
   commensalCalTimer_.startTimer(recalPeriodSecs, this,
                                 &ObsActStrategy::requestCommensalCal,
                                 getVerboseLevel());
}

void ObsActStrategy::configureRotatePrimaryTargetIdsTimer()
{
   const double SecsPerMin(60);

   int rotatePeriodSecs(nssParameters_.sched_->getRotatePrimaryTargetIdsIntervalMinutes()
                        * SecsPerMin);
   
   rotatePrimaryTargetIdsTimer_.setRepeatIntervalSecs(rotatePeriodSecs);
   
   rotatePrimaryTargetIdsTimer_.startTimer(rotatePeriodSecs, this,
                                          &ObsActStrategy::requestRotatePrimaryTargetIds,
                                          getVerboseLevel());
}


void ObsActStrategy::startInternalHook()
{
   if (nssParameters_.sched_->commensalCalEnabled())
   {
      configureCommensalCalTimer();
   }

   if (nssParameters_.sched_->rotatePrimaryTargetIdsEnabled())
   {
      configureRotatePrimaryTargetIdsTimer();
   }
}


bool ObsActStrategy::isCommensalCalPending()
{
   return commensalCalPending_.value();
}

void ObsActStrategy::requestCommensalCal()
{
   commensalCalPending_ = true;

   SseArchive::SystemLog() << "Commensal calibration request is pending..." << endl;
}

bool ObsActStrategy::isRotatePrimaryTargetIdsPending()
{
   return rotatePrimaryTargetIdsPending_.value();
}

void ObsActStrategy::requestRotatePrimaryTargetIds()
{
   rotatePrimaryTargetIdsPending_ = true;

   SseArchive::SystemLog() << "Rotation of primary target IDs is pending..." << endl;
}

static double TotalDxBandwidthMhz(const DxList &dxList)
{
   double total(0);

   for(DxList::const_iterator it = dxList.begin();
       it != dxList.end(); ++it)
   {
      DxProxy *dxProxy = *it;
      total += dxProxy->getBandwidthInMHz();
   }

   return total;
}

// Copy the dx sky freqs from the dxs on the beam associated
// with sourceBeamName to the dxs on the other beams in the
// dxListByBeamMap.  Unused dxs on the other beams will be
// tuned to a skyfreq of -1 Mhz.

void ObsActStrategy::copyDxTuningsFromOneBeamToTheOthers(
      DxListByBeamMap & dxListByBeamMap,
      const string & sourceBeamName)
{
   Assert(dxListByBeamMap.size() > 1);

   // make sure specified source beam is in the map
   Assert(dxListByBeamMap.find(sourceBeamName) != dxListByBeamMap.end());

   DxList & sourceDxList = dxListByBeamMap[sourceBeamName];
   
   for (DxListByBeamMap::iterator it = dxListByBeamMap.begin();
	it != dxListByBeamMap.end(); ++it)
   {
      const string & destBeamName = it->first;
      
      // don't copy the source to itself
      if (destBeamName != sourceBeamName)
      {
	 DxList & destDxList = it->second;

	 // set sky freq to dummy value, to mark any dxs not to be used
	 for (DxList::iterator listIndex = destDxList.begin();
	      listIndex != destDxList.end();  ++listIndex)
	 {
	    (*listIndex)->setDxSkyFreq(-1);
	 }

	 // copy the tunings
	 for(DxList::iterator sourceIndex = sourceDxList.begin(),
		destIndex = destDxList.begin();
	     sourceIndex != sourceDxList.end() && destIndex != destDxList.end();
	     ++sourceIndex, ++destIndex)
	 {
	    DxProxy *sourceDxProxy = *sourceIndex;
	    DxProxy *destDxProxy = *destIndex;

	    double skyFreqMhz = sourceDxProxy->getDxSkyFreq();
	    destDxProxy->setDxSkyFreq(skyFreqMhz);
            int32_t chanNumber = sourceDxProxy->getChannelNumber();
            destDxProxy->setChannelNumber(chanNumber);
	 }
      }      
   }
}


// Copy the zx sky freqs from the zxs on the beam associated
// with sourceBeamName to the zxs on the other beams in the
// zxListByBeamMap.  Unused zxs on the other beams will be
// tuned to a skyfreq of -1 Mhz.

void ObsActStrategy::copyZxTuningsFromOneBeamToTheOthers(
      DxListByBeamMap & zxListByBeamMap,
      const string & sourceBeamName)
{
   Assert(zxListByBeamMap.size() > 1);

   // make sure specified source beam is in the map
   Assert(zxListByBeamMap.find(sourceBeamName) != zxListByBeamMap.end());

   DxList & sourceZxList = zxListByBeamMap[sourceBeamName];
   
   for (DxListByBeamMap::iterator it = zxListByBeamMap.begin();
	it != zxListByBeamMap.end(); ++it)
   {
      const string & destBeamName = it->first;
      
      // don't copy the source to itself
      if (destBeamName != sourceBeamName)
      {
	 DxList & destZxList = it->second;

	 // set sky freq to dummy value, to mark any zxs not to be used
	 for (DxList::iterator listIndex = destZxList.begin();
	      listIndex != destZxList.end();  ++listIndex)
	 {
	    (*listIndex)->setDxSkyFreq(-1);
	 }

	 // copy the tunings
	 for(DxList::iterator sourceIndex = sourceZxList.begin(),
		destIndex = destZxList.begin();
	     sourceIndex != sourceZxList.end() && destIndex != destZxList.end();
	     ++sourceIndex, ++destIndex)
	 {
	    DxProxy *sourceZxProxy = *sourceIndex;
	    DxProxy *destZxProxy = *destIndex;

	    double skyFreqMhz = sourceZxProxy->getDxSkyFreq();
	    destZxProxy->setDxSkyFreq(skyFreqMhz);
            int32_t chanNumber = sourceZxProxy->getChannelNumber();
            destZxProxy->setChannelNumber(chanNumber);
	 }
      }      
   }
}

// Select the first targetId, and assign it to the parameter
// associated with the beamName.  Also set the primaryTargetId,
// if required.

void ObsActStrategy::chooseTargets(const string & firstTargetBeamName,
                                   const DxList &dxList)
{
   string methodName("ObsActStrategy::chooseTargets: ");

   SseArchive::SystemLog() << "Target selection mode: "
                           << nssParameters_.sched_->getChooseTargetOptionString()
                           << endl;

   if (nssParameters_.sched_->getChooseTargetOption() != 
       SchedulerParameters::CHOOSE_TARGET_USER)
   {
      double bandwidthOfSmallestDxMhz =
	 MinBandwidth<DxList::const_iterator, DxProxy>(
	    dxList.begin(),
	    dxList.end());
  
      VERBOSE2(getVerboseLevel(), methodName 
	       << " bandwidthOfSmallestDxMhz = "
	       << bandwidthOfSmallestDxMhz << endl);

      if (bandwidthOfSmallestDxMhz <= 0)
      {
	 throw SseException(
	    "Error: one or more dxs has a bandwidth <= 0\n",
            __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }
  
      // specify the minimum remaining-to-be-observed 
      // bandwidth that's acceptable for the selected target
      // (ie, use at least X% of the total available dx bandwidth)
  
      const double minDecimalPercentDxBwToBeUsed =
	 nssParameters_.sched_->getMinDxPercentBwToUse() / 100;
  
      double minAcceptableRemainingBandMhz = 
         max(bandwidthOfSmallestDxMhz,
             TotalDxBandwidthMhz(dxList) * minDecimalPercentDxBwToBeUsed);
      Assert (minAcceptableRemainingBandMhz > 0);


      VERBOSE2(getVerboseLevel(), 
	       methodName << "minAcceptableRemainingBandMhz = " 
	       << minAcceptableRemainingBandMhz << endl);

      ActivityIdList activityIdList; 
      getRunningActivityIds(activityIdList);

      TargetId firstTargetId;
      TargetId primaryTargetId;
      ObsRange chosenObsRange;
      TargetIdSet additionalTargetIds;

      vector<string> beamNamesToUse = nssParameters_.sched_->getBeamsToUse();
      int nTargetsToChoose = 1;
      if (nssParameters_.sched_->useMultipleTargets())
      {
         nTargetsToChoose = beamNamesToUse.size();
      }

      bool areActsRunning(activityIdList.size() > 0);

      chooseAutoTargetsFromDb(
         nTargetsToChoose,
	 bandwidthOfSmallestDxMhz,
	 minAcceptableRemainingBandMhz,
	 nssParameters_.sched_->getMinTargetSepBeamsizes(),
	 areActsRunning,
	 firstTargetId,
         chosenObsRange,
	 primaryTargetId,
         additionalTargetIds
         );

      delete tuneDxs_;

      tuneDxs_ =
	 new TuneDxsObsRange(getVerboseLevel(), chosenObsRange);
      VERBOSE2(getVerboseLevel(), methodName 
	       << "Chose first target: " << firstTargetId << 
	       " for beam: " << firstTargetBeamName << endl;);

      VERBOSE2(getVerboseLevel(), methodName 
	       << "Chose primary target: " << primaryTargetId << endl;);
      nssParameters_.act_->setTargetIdForBeam(firstTargetBeamName, firstTargetId);
      if (firstTargetId != nssParameters_.act_->getTargetIdForBeam(firstTargetBeamName))
      {
	 throw SseException(
	    "Invalid first target id\n", __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }

      if (nssParameters_.act_->getPrimaryBeamPositionType() == 
          ActParameters::PRIMARY_BEAM_POS_TARGET_ID)
      {
         nssParameters_.act_->setPrimaryTargetId(primaryTargetId);
         if (primaryTargetId != nssParameters_.act_->getPrimaryTargetId())
         {
            throw SseException(
               "Invalid primary target id\n", __FILE__, __LINE__,
               SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
         }
      }

      if (nTargetsToChoose > 1)
      {
         unsigned int nAdditionalTargetsExpected = nTargetsToChoose -1;
         if (additionalTargetIds.size() < nAdditionalTargetsExpected)
         {
            throw SseException(
               "could not find enough additional targets\n", __FILE__, __LINE__,
               SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
         }
         
         assignAdditionalTargetsToBeams(firstTargetBeamName, 
                                        additionalTargetIds);

      }
//
//	Now that we have the targets
//	check what frequencies that the zxs have already observed on this target.
//	Remove the observed frequencies from the the badBandsForZxs.
//
// 	Convert the permanent RFI mask to an ObsRange for the Zxs
   PermRfiMask permRfiMask(PermRfiMaskFilename, "permRfiMask", 
                           getVerboseLevel());
	 
   vector<FrequencyBand> permRfiBands(permRfiMask.getFreqBands());

	ObsRange badBandsForZxs;
	badBandsForZxs.convertFreqBandToObsRange(permRfiBands);
if (badBandsForZxs.isEmpty() )cout << "badBandsForZxs is empty" << endl;
	delete tuneZxs_;
	ObsRange zxObservedRanges;
	orderedTargets_->retrieveObsHistFromDbForZxs(zxObservedRanges, firstTargetId);
	badBandsForZxs = badBandsForZxs - zxObservedRanges;
	tuneZxs_ =
		new TuneZxsObsRange(getVerboseLevel(),badBandsForZxs);
   }
}


// Assign targets to the remaining beams.
// Targets are stored in the sched params associated with the beamNames.

void ObsActStrategy::assignAdditionalTargetsToBeams(
   const string & firstChosenBeamName,
   const TargetIdSet & additionalTargetIds)
{
   string methodName("ObsActStrategy::assignAdditionalTargetsToBeams(): ");

   Assert(nssParameters_.sched_->getChooseTargetOption() != 
          SchedulerParameters::CHOOSE_TARGET_USER);

   vector<string> beamNamesToUse = nssParameters_.sched_->getBeamsToUse();
   
   // remove the name of the first chosen beam since it's already got a target
   vector<string>::iterator firstChosenBeamNameIt = 
      find(beamNamesToUse.begin(), beamNamesToUse.end(), firstChosenBeamName);
   Assert(firstChosenBeamNameIt != beamNamesToUse.end());
   beamNamesToUse.erase(firstChosenBeamNameIt);
   
   unsigned int nTargetsToAssign = beamNamesToUse.size();
   Assert(nTargetsToAssign > 0);
   Assert(nTargetsToAssign == additionalTargetIds.size());
   
   // Assign each chosen target to a remaining beam
   vector<string>::iterator beamNameIt = beamNamesToUse.begin();
   for (TargetIdSet::const_iterator targetIdIt = additionalTargetIds.begin();
        targetIdIt != additionalTargetIds.end(); ++targetIdIt)
   {
      const TargetId & targetId = *targetIdIt;
      
      Assert(beamNameIt != beamNamesToUse.end());
      const string & beamName = *beamNameIt;
      
      VERBOSE2(getVerboseLevel(), methodName << "Assigned target: " << targetId << 
               " to beam: " << beamName << endl;);
      
      nssParameters_.act_->setTargetIdForBeam(beamName, targetId);
      if (targetId != nssParameters_.act_->getTargetIdForBeam(beamName))
      {
         throw SseException(
            "Invalid target number\n", __FILE__, __LINE__,
            SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }
      
      SseArchive::SystemLog() << "Assigned additional target: " << targetId 
                              << " to beam: " << beamName << endl;
      
      ++beamNameIt; 
   }
}


/*
  Tune dxs in series, across all requested beams.
 */
void ObsActStrategy::tuneDxsForMultipleBeamsOnSingleTarget(
   NssComponentTree *nssComponentTree)
{
VERBOSE2( getVerboseLevel(),"tuneDxsForMuptipleBeamsOnSingleTarget()" << endl);
   // Get all the beams requested overall
   vector<string> allBeamsToUse(nssParameters_.sched_->getBeamsToUse());
   sort(allBeamsToUse.begin(), allBeamsToUse.end());

   //ExpectedNssComponentsTree *expectedTree = 
    // nssComponentTree->getExpectedNssComponentsTree();

#ifdef prelude
   // Get ifcs in use
   IfcList ifcList = nssComponentTree->getIfcsForBeams(
      nssParameters_.sched_->getBeamsToUse()); 
   

   for (IfcList::iterator ifcIt = ifcList.begin(); 
	ifcIt != ifcList.end(); ++ifcIt)
   {
      IfcProxy *ifcProxy = *ifcIt;

      // Only use the dxs for the beams on this ifc that have
      // been requested for use.
      // First get all the beam names associated with this ifc:
      // (Note: these are beam1, beam2, etc., not the ATA beam names)

      vector<string> beamNamesForIfc(
	 expectedTree->getBeamsForIfc(ifcProxy->getName()));
      sort(beamNamesForIfc.begin(), beamNamesForIfc.end());

      // Now find out which beams have been requested
      vector<string> requestedBeamNamesForIfc;
      set_intersection(beamNamesForIfc.begin(), beamNamesForIfc.end(),
		       allBeamsToUse.begin(), allBeamsToUse.end(),
		       back_inserter(requestedBeamNamesForIfc));

   }
#endif
   // Get list of Channelizers
         ChannelizerList chanList = 
		nssComponentTree->getChansForBeams(nssParameters_.sched_->getBeamsToUse());
  if (chanList.begin() == chanList.end() )
  VERBOSE2(getVerboseLevel(), " Chanlist empty "  << endl);
   // One channelizer is good enough. They all better be tuned the same.
         ChannelizerProxy *chanProxy = *chanList.begin(); 
          int32_t outputChannels = chanProxy->getOutputChannels();
          float64_t mhzPerChannel = chanProxy->getMhzPerChannel();
 
   float64_t  maxAllowedDxSkyFreqMhz = 99999;  // effectively no limit
      // Get the dxs and tune them
      DxList dxListForIfc =
	 nssComponentTree->getDxsForBeams(allBeamsToUse);
      Assert(tuneDxs_);
	tuneDxs_->tune(dxListForIfc, outputChannels, mhzPerChannel);
      float64_t channelizerTuneFreqMhz = computeChanCenterFreq(dxListForIfc, outputChannels,
		mhzPerChannel); 
       
       int32_t delaySecs = nssParameters_.chan_->getDelay();
       nssParameters_.chan_->start( delaySecs, channelizerTuneFreqMhz, "all");
      /* 
         Assign channels to each dx, adjusting frequencies to
         fit the channel centers as needed.

         TBD: expand this to handle the auto tune case.
       */

      if (! nssParameters_.sched_->isAutoTuneRf() &&
          nssParameters_.sched_->getTuneDxsOption() == SchedulerParameters::TUNE_DXS_RANGE)
      {
         /*
           TBD
           - get bandwidth from channelizer directly
           - figure out which tuning applies to this beam
           - Assume beamformer bandwidth is set to the chanzer BW for now
         */
         //double chanzerWidthMhz(
          //  nssParameters_.sched_->getBeamBandwidthMhz());
         
         //const string tuningName("tuningc");
         //double chanzerTuneMhz = nssParameters_.tscope_->getTuningSkyFreqMhz(
            //tuningName);

         //channelizeDxTunings(dxListForIfc, chanzerTuneMhz, chanzerWidthMhz);
      }

      /*
        Assume for now that all dxs are on the same tuning
        and assigned freqs must fit within the beam bandwidth requirement.
        TBD: check tuning assignments to free this restriction.
      */
      double minAssignedDxFreqMhz = MinDxSkyFreqMhz(dxListForIfc);

      maxAllowedDxSkyFreqMhz = min(
          maxAllowedDxSkyFreqMhz,
          minAssignedDxFreqMhz + nssParameters_.sched_->getBeamBandwidthMhz());

}

/*
  Assign channels to the dxs based on their currently assigned
  frequencies and the channelizer center freq and width.
  The dx frequencies will be adjusted as needed so that
  they match the associated channel.

  Assumes that the channelizer has N channels, and that
  the channelizer tune freq refers to the center of channel
  N/2.

 */
// Obsolete
void ObsActStrategy::channelizeDxTunings(DxList & dxList,
                                          double chanzerTuneMhz,
                                          double chanzerWidthMhz)
{
   Assert(! dxList.empty());
   double singleChanWidthMhz = dxList.front()->getBandwidthInMHz();
   int totalChans = static_cast<int>((chanzerWidthMhz / singleChanWidthMhz) + 0.5);
   int centerChan = totalChans / 2;

   for (DxList::iterator dxIt = dxList.begin();
	dxIt != dxList.end(); ++dxIt)
   {
      DxProxy *dxProxy = *dxIt;

      // is dx in use?
      if (dxProxy->getDxSkyFreq() > 0)
      {
         // determine appropriate channel
         double chanOffset = (chanzerTuneMhz - dxProxy->getDxSkyFreq()) /
            singleChanWidthMhz;

         int chanNumber = (static_cast<double>(centerChan) - chanOffset + 0.5);
         if (chanNumber < 0 || chanNumber >= totalChans)
         {
            stringstream strm;
            strm << " Attempted to tune dx to " << dxProxy->getDxSkyFreq()
                 << " MHz " << "which is outside the " << chanzerWidthMhz
                 << " MHz " << "wide channelizer band tuned to " 
                 << chanzerTuneMhz << " MHz. "
                 << "Check beginfreq and/or endfreq parameters."
                 << endl;

            throw SseException(strm.str(), __FILE__, __LINE__, 
                               SSE_MSG_INVALID_PARMS, SEVERITY_ERROR);
         }

         dxProxy->setChannelNumber(chanNumber);

         // adjust dx tune freq to match the channel center
         double tuneOffsetMhz = (centerChan - chanNumber) * singleChanWidthMhz;
         dxProxy->setDxSkyFreq(chanzerTuneMhz - tuneOffsetMhz);
      }
   }
}

void ObsActStrategy::pickMultipleTargetsAndTuneDxs(NssComponentTree *nssComponentTree)
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::pickMultipleTargetsAndTuneDxs()" << endl;);

   // verify that enough beams have been requested
   vector<string> beamsToUse(nssParameters_.sched_->getBeamsToUse());
   unsigned int minNumberOfBeams(2);      
   if (beamsToUse.size() < minNumberOfBeams)
   {
      stringstream strm;
      strm << "Must specify at least " << minNumberOfBeams << " beams for "
	   << "multiple target mode\n";

      throw SseException(strm.str(), __FILE__, __LINE__, 
			 SSE_MSG_MISSING_BEAM, SEVERITY_ERROR);
   }

// Get dx List by beam and find the beam with the shortest list

   DxListByBeamMap dxListByBeamMap;
   getDxListForEachBeam(nssComponentTree, beamsToUse, dxListByBeamMap);
   Assert(dxListByBeamMap.size() == beamsToUse.size());

   // Make sure each beam has at least 1 dx
   for (DxListByBeamMap::iterator it = dxListByBeamMap.begin();
	it != dxListByBeamMap.end(); ++it)
   {
      const string & beamName = it->first;
      DxList & dxList = it->second;
      if (dxList.size() == 0)
      {
	 stringstream strm;
	 strm << "No dxs are available for beam " << beamName << "\n";
	 throw SseException(strm.str(), __FILE__, __LINE__,
			    SSE_MSG_MISSING_DX, SEVERITY_ERROR);
      }
   }
    
   // Start with the beam that has the fewest dxs.
   // Pick a target for it and tune those dxs.

   string shortestListBeamName;
   DxList shortestDxList = getShortestDxList(dxListByBeamMap,
						shortestListBeamName);

// Do the same for the zxs

   DxListByBeamMap zxListByBeamMap;
   getZxListForEachBeam(nssComponentTree, beamsToUse, zxListByBeamMap);
   Assert(zxListByBeamMap.size() == beamsToUse.size());

   // Make sure each beam has at least 1 dx
   for (DxListByBeamMap::iterator it = zxListByBeamMap.begin();
	it != zxListByBeamMap.end(); ++it)
   {
      const string & beamName = it->first;
      DxList & zxList = it->second;
      if (zxList.size() == 0)
      {
	 stringstream strm;
	 strm << "No zxs are available for beam " << beamName << "\n";
	 throw SseException(strm.str(), __FILE__, __LINE__,
			    SSE_MSG_MISSING_DX, SEVERITY_ERROR);
      }
   }
    
   // Start with the beam that has the fewest dxs.
   // Pick a target for it and tune those dxs.

   string shortestZxListBeamName;
   DxList shortestZxList = getShortestDxList(zxListByBeamMap,
						shortestZxListBeamName);
// We may not want to throw an exception.

   chooseTargets(shortestListBeamName, shortestDxList);

   // Get list of Channelizers
         ChannelizerList chanList = 
		nssComponentTree->getChansForBeams(nssParameters_.sched_->getBeamsToUse());
  if (chanList.begin() == chanList.end() )
  VERBOSE2(getVerboseLevel(), " Chanlist empty "  << endl);
   // One channelizer is good enough. They all better be tuned the same.
         ChannelizerProxy *chanProxy = *chanList.begin(); 
          int32_t outputChannels = chanProxy->getOutputChannels();
          float64_t mhzPerChannel = chanProxy->getMhzPerChannel();
 
   
   // Tune the dxs on one beam, then copy tunings to the others
   // double maxAllowedDxSkyFreqMhz = 99999;  // effectively no limit
   Assert(tuneDxs_);
	tuneDxs_->tune(shortestDxList, outputChannels, mhzPerChannel);

      float64_t channelizerTuneFreqMhz = computeChanCenterFreq(shortestDxList, outputChannels,
		mhzPerChannel); 

// Now tune the Zxs
// Get zx List by beam and find the beam with the shortest list
// Need to get the TargetId for tune beam.
// Check observed frequencies for zxs on that beam
// remove observed frequencies for the ObsRange
       ObsRange zxObservedRanges;
       TargetId firstTargetId = nssParameters_.act_->getTargetIdForBeam(
		       shortestListBeamName);
	orderedTargets_->retrieveObsHistFromDbForZxs(zxObservedRanges, firstTargetId);
	tuneZxs_->tune(shortestZxList, outputChannels, mhzPerChannel,
		channelizerTuneFreqMhz, zxObservedRanges );

       int32_t delaySecs = nssParameters_.chan_->getDelay();
       nssParameters_.chan_->start( delaySecs, channelizerTuneFreqMhz, "all");
   copyDxTuningsFromOneBeamToTheOthers(dxListByBeamMap,
					shortestListBeamName);
   copyZxTuningsFromOneBeamToTheOthers(zxListByBeamMap,
					shortestZxListBeamName);
}

// For each beam in the beamNames list, add a DxList
// entry in the DxListByBeamMap map.  If there are no
// dxs available for that beam, then that list will be empty.

void ObsActStrategy::getDxListForEachBeam(
   NssComponentTree *nssComponentTree, 
   vector<string> & requestedBeamNames, 
   DxListByBeamMap & dxListByBeamMap)
{
   for (vector<string>::iterator it = requestedBeamNames.begin();
	it != requestedBeamNames.end(); ++it)
   {
      const string & beamName = *it;
	    
      vector<string> beamSublist;
      beamSublist.push_back(beamName);
	    
      DxList dxsForBeam =
	 nssComponentTree->getDxsForBeams(beamSublist);
	 
      dxListByBeamMap[beamName] = dxsForBeam;
   }
}

// For each beam in the beamNames list, add a DxList
// entry in the DxListByBeamMap map.  If there are no
// dxs available for that beam, then that list will be empty.

void ObsActStrategy::getZxListForEachBeam(
   NssComponentTree *nssComponentTree, 
   vector<string> & requestedBeamNames, 
   DxListByBeamMap & zxListByBeamMap)
{
   for (vector<string>::iterator it = requestedBeamNames.begin();
	it != requestedBeamNames.end(); ++it)
   {
      const string & beamName = *it;
      vector<string> beamSublist;
      beamSublist.push_back(beamName);
      DxList zxsForBeam =
	 nssComponentTree->getZxsForBeams(beamSublist);
      zxListByBeamMap[beamName] = zxsForBeam;
   }
}

struct BeamInfo
{
   string beamName;
   int numberOfDxs;
};

static bool CompareBeamInfoByNumberOfDxs(BeamInfo elem1,
					  BeamInfo elem2)
{
   return elem1.numberOfDxs < elem2.numberOfDxs;
}

// Return dx list that has the fewest dxs along with its beamName
DxList ObsActStrategy::getShortestDxList(
   DxListByBeamMap & dxListByBeamMap,
   string & shortestListBeamName)
{
   list<BeamInfo> beamInfoList;
   for (DxListByBeamMap::iterator it = dxListByBeamMap.begin();
	it != dxListByBeamMap.end(); ++it)
   {
      BeamInfo beamInfo;
      beamInfo.beamName = it->first;
      beamInfo.numberOfDxs = it->second.size();

      beamInfoList.push_back(beamInfo);
   }

   Assert(beamInfoList.size() > 0);
   BeamInfo minBeamInfo = * min_element(beamInfoList.begin(),
                                        beamInfoList.end(),
                                        CompareBeamInfoByNumberOfDxs);

   shortestListBeamName = minBeamInfo.beamName;

   return dxListByBeamMap[minBeamInfo.beamName];

}

void ObsActStrategy::pickSingleTargetAndTuneDxs(
   NssComponentTree *nssComponentTree)
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::pickSingleTargetAndTuneDxs()" << endl;);

   // Single target, possibly multiple beams.  Point each beam
   // at the target, tune the dxs in series to get maximum
   // frequency coverage.

   vector<string> beamsToUse(nssParameters_.sched_->getBeamsToUse());
   unsigned int minNumberOfBeams(1);  
   if (beamsToUse.size() < minNumberOfBeams)
   {
      stringstream strm;
      strm << "Must specify at least " << minNumberOfBeams << " beam(s)\n";

      throw SseException(strm.str(), __FILE__, __LINE__, 
			 SSE_MSG_MISSING_BEAM, SEVERITY_ERROR);
   }

   DxList dxList = nssComponentTree->getDxsForBeams(beamsToUse);
   string firstBeamName(*beamsToUse.begin());
   chooseTargets(firstBeamName, dxList);
    
   TargetId targetId = nssParameters_.act_->getTargetIdForBeam(firstBeamName);

   // assign targetId to all requested beams
   for (vector<string>::iterator beamNameIt = beamsToUse.begin();
	beamNameIt != beamsToUse.end(); ++beamNameIt)
   {
      const string & beamName = *beamNameIt;

      nssParameters_.act_->setTargetIdForBeam(beamName, targetId);
      if (targetId != nssParameters_.act_->getTargetIdForBeam(beamName))
      {
	 throw SseException(
	    "Invalid target number\n", __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }
   }

   tuneDxsForMultipleBeamsOnSingleTarget(nssComponentTree);
}

void ObsActStrategy::updateObservedFrequencies(ActivityId_t actId)
{
   // Update freqs (in memory) for automatically selected targets.

   if (nssParameters_.sched_->getChooseTargetOption() != 
       SchedulerParameters::CHOOSE_TARGET_USER)
   {
      Assert(nssParameters_.db_->useDb());

      // Might get here in followup only mode (i.e., the scheduler did not 
      // actually choose the target) so verify that orderedTargets_ is valid.

      if (orderedTargets_)
      {
	 orderedTargets_->updateObservedFreqsForTargetsFromDbObsHistory(actId);
      }
   }
}
 
/*
  See if there's enough time left to do a full data collection.
*/
bool ObsActStrategy::isTargetTooCloseToSetting(
   time_t currentTime, time_t setTime)
{
   bool tooCloseToSetting(false);
   
   int minAcceptableRemainingTimeSecs(
      nssParameters_.sched_->getTargetAvailActSetupTimeSecs()  
      + nssParameters_.dx_->getDataCollectionLengthSecs());
   
   if ((setTime - currentTime) < minAcceptableRemainingTimeSecs)
   {
      tooCloseToSetting = true;
   }

   return tooCloseToSetting;
}

TargetId ObsActStrategy::chooseCommensalCalTargetId()
{
   /*
     Use NRAO530 if it's up, otherwise pick whatever cal target
     is available.
    */
   TargetId nrao530SetiTargetId(510);
   bool nrao530IsVisible(false);
   int nrao530TargetIndex(-1);

   /*
     Find the cal target with the max flux at the
     (already selected) cal freq, that's up at least the min amount of time.
   */
   double maxFluxAtCalFreq(-1);
   int targetIndexWithMaxFlux(-1);
   const double minUpTimeForCalHours(0.25); // tbd get from params

   const vector<TargetInfo> & targetInfoVect(calTargets_.getTargetInfo());
   TscopeParameters * tscope(nssParameters_.tscope_);

   // use current time
   time_t obsTime;
   time(&obsTime);

   for (unsigned int i=0; i<targetInfoVect.size(); ++i)
   {
      double riseHoursUtc, transitHoursUtc, setHoursUtc;
      double untilRiseHours, untilSetHours;

      SseAstro::riseTransitSet(targetInfoVect[i].ra2000Hours,
                               targetInfoVect[i].dec2000Deg,
                               tscope->getSiteLongWestDeg(),
                               tscope->getSiteLatNorthDeg(),
                               tscope->getSiteHorizDeg(), 
                               obsTime,
                               &riseHoursUtc,
                               &transitHoursUtc,
                               &setHoursUtc,
                               &untilRiseHours,
                               &untilSetHours);
            
      // Find best cal target that's up
      if (untilSetHours > minUpTimeForCalHours)
      {
         if (targetInfoVect[i].targetId == nrao530SetiTargetId)
         {
            nrao530IsVisible = true;
            nrao530TargetIndex = i;
         }

         if (targetInfoVect[i].fluxJy > maxFluxAtCalFreq)
         {
            maxFluxAtCalFreq = targetInfoVect[i].fluxJy;
            targetIndexWithMaxFlux = i;
         }
      }
   }

   if (targetIndexWithMaxFlux < 0)
   {
      throw SseException("No commensal cal targets match specified freq, site info, "
                         + string("& minimum uptime\n"),
                         __FILE__, __LINE__ );
   }

   const TargetInfo *chosenTargetInfo;
   if (nrao530IsVisible)
   {
      chosenTargetInfo = &targetInfoVect[nrao530TargetIndex];
   }
   else
   {
      chosenTargetInfo = &targetInfoVect[targetIndexWithMaxFlux];
   }

   SseArchive::SystemLog() 
      << "Commensal Cal target: " 
      << chosenTargetInfo->targetId
      << " (" << chosenTargetInfo->name
      << "), estimated flux: " << chosenTargetInfo->fluxJy << " Jy" 
      << " @ " << AssumedCommensalCalFreqMhz << " MHz " 
      << endl;

   return chosenTargetInfo->targetId;
}

Activity *ObsActStrategy::getCommensalCalActivity(NssComponentTree *nssComponentTree)
{
   string methodName("ObsActStrategy::getCommensalCalActivity()");
   VERBOSE2(getVerboseLevel(), methodName << endl;);

   /*
     Use a copy of the parameters so it doesn't adversely affect
     regular observing.
   */
   auto_ptr<NssParameters> nssParams(new NssParameters(nssParameters_));

   // set activity type (for logging purposes)
   string actType("pointantswait");
   if (! nssParams->act_->setActivityType(actType))
   {
      throw SseException("tried to set invalid activity type: " + actType
                         + " in " + methodName + "\n", __FILE__, __LINE__);
   }    

   // TBD
   // - set params: set point wait time (new act param?)

   nssParams->act_->setPrimaryTargetId(chooseCommensalCalTargetId());
   nssParams->act_->setPrimaryBeamPositionType(ActParameters::PRIMARY_BEAM_POS_TARGET_ID);

   Activity *activity = NewPointAntsAndWaitActWrapper(
      getNextActId(), this, nssComponentTree, *nssParams, getVerboseLevel());

   return activity;
}


Activity *ObsActStrategy::getNonFollowupActivity(NssComponentTree *nssComponentTree)
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::getNonFollowupActivity()" << endl;);

   if (nssParameters_.sched_->useMultipleTargets())
   {
      pickMultipleTargetsAndTuneDxs(nssComponentTree);
   }
   else 
   {
      pickSingleTargetAndTuneDxs(nssComponentTree);
   }

   bool allAvailable;
   bool allInPrimaryFov;
   validateChosenTargets(nssParameters_, &allAvailable, &allInPrimaryFov);
			 
   if (allAvailable & allInPrimaryFov)
   {
      VERBOSE2(getVerboseLevel(), "ObsActStrategy::getNonFollowupActivity(), "
	       "calling IdNumberFactory" << endl;);
      
      ActivityId_t actId = getNextActId();
      
      Activity *activity =
	 scheduler_->getNewActivityFromFactory(
            actId, this, nssComponentTree, nssParameters_, getVerboseLevel());

      return activity;
   }
   else 
   {
      stringstream msg;
      msg << "Invalid targets for activity type: "
          <<  nssParameters_.act_->getActivityType() << endl;
      
      throw SseException(msg.str(), __FILE__, __LINE__,
                      SSE_MSG_INVALID_TARGET, SEVERITY_ERROR);
   }
}


/*
  Assume targets have already passed validity check
 */
Activity *ObsActStrategy::getFollowupActivity(const NssParameters &nssParams,
                                              NssComponentTree *nssComponentTree)
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::getFollowupActivity()" << endl;);

   vector<string> beamsToUse(nssParams.sched_->getBeamsToUse());
   DxList dxList = nssComponentTree->getDxsForBeams(beamsToUse);
   
   TuneDxs::tuneDxsFromPrevActInDatabase(
      nssParams.act_->getPreviousActivityId(),
      nssParams.db_->getDb(),
      dxList);
   
   DxList zxList = nssComponentTree->getZxsForBeams(beamsToUse);
   
   TuneZxs::tuneZxsFromPrevActInDatabase(
      nssParams.act_->getPreviousActivityId(),
      nssParams.db_->getDb(),
      zxList);
   
   // Get list of Channelizers
    ChannelizerList chanList = 
     nssComponentTree->getChansForBeams(nssParameters_.sched_->getBeamsToUse());
     if (chanList.begin() == chanList.end() )
  	VERBOSE2(getVerboseLevel(), " Chanlist empty "  << endl);
   // One channelizer is good enough. They all better be tuned the same.
         ChannelizerProxy *chanProxy = *chanList.begin(); 
          int32_t outputChannels = chanProxy->getOutputChannels();
          float64_t mhzPerChannel = chanProxy->getMhzPerChannel();
      float64_t channelizerTuneFreqMhz = 
	computeChanCenterFreq(dxList, outputChannels, mhzPerChannel);
       int32_t delaySecs = nssParameters_.chan_->getDelay();
       nssParameters_.chan_->start( delaySecs, channelizerTuneFreqMhz, "all");

   ActivityId_t actId = getNextActId();
   
   // Let activity make its own copy of the parameters
   Activity *activity =
      scheduler_->getNewActivityFromFactory(
         actId, this, nssComponentTree, nssParams, getVerboseLevel());

   return activity;
   
}

/*
  Try to create a followup activity:
  If successful, return activity object.
  If can't create a followup activity, but not for any nonrecoverable reason,
  return 0;
  If nonrecoverable error occurs, throw SseException.

  TBD: change this code to handle a primary FOV that is selected by
coords and not a target ID.
 */

Activity * ObsActStrategy::prepareValidFollowupActivity(
   NssComponentTree *nssComponentTree)
{
   const string methodName("ObsActStrategy::prepareValidFollowupActivity() ");
   VERBOSE2(getVerboseLevel(), methodName << "followupIsPending" << endl;);
   
   // Make private copy of nss parameters so they can be modified for followups
   const auto_ptr<NssParameters> followupNssParameters(
      new NssParameters(nssParameters_));

   followup_->prepareParameters(followupNssParameters.get());

   bool allFollowupTargetsAvailable;
   bool allFollowupTargetsInPrimaryFov;
   validateChosenTargets(*followupNssParameters.get(),
                         &allFollowupTargetsAvailable,
                         &allFollowupTargetsInPrimaryFov);

   if (allFollowupTargetsAvailable && allFollowupTargetsInPrimaryFov)
   {
      return getFollowupActivity(*followupNssParameters.get(),
                                 nssComponentTree);
   } 
   else  // invalid targets for followup
   {
      // TBD - how should this be handled in the various target modes?
      if (! allFollowupTargetsInPrimaryFov)
      {
         SseMessage::log(
            MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INVALID_TARGET,
            SEVERITY_ERROR, "Skipping followup: targets outside primary FOV\n",
            __FILE__, __LINE__);
	       
         return 0;
      }

      if (! allFollowupTargetsAvailable)
      {
         if (nssParameters_.sched_->getChooseTargetOption() == 
             SchedulerParameters::CHOOSE_TARGET_USER)
         {
            throw SseException(
               "Skipping followup: not all targets are available\n",
               __FILE__, __LINE__);
         }
         else if (nssParameters_.sched_->getChooseTargetOption() == 
                  SchedulerParameters::CHOOSE_TARGET_SEMIAUTO)
         {
            throw SseException(
               "Skipping followup: not all targets are available\n",
               __FILE__, __LINE__);
         }
         else  // auto target or commensal modes, followup targets not visible
         {
            SseMessage::log(
               MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INVALID_TARGET,
               SEVERITY_WARNING, "Skipping followup: not all targets are available\n",
               __FILE__, __LINE__);
            
            if (isWrapUpRequested())
            {
               throw SseException(
                  "Could not do followup and wrapup was requested\n",
                  __FILE__, __LINE__, SSE_MSG_INVALID_TARGET, SEVERITY_ERROR);
            }
            
            return 0;
         }
      }

   }

   return 0;
}

/*
  Returns next appropriate activity.
  Throws exceptions if there are no more activities to run,
  or if there's an error trying to create an activity:
     ActivityStartWaitingForTargetComplete
     ActivityStartTerminatedDueToWrapupException
     SseException
*/
Activity *ObsActStrategy::getNextActivity(NssComponentTree *nssComponentTree)
{
   const string methodName("ObsActStrategy::getNextActivity() ");
   VERBOSE2(getVerboseLevel(), methodName << endl;);

   if (nssParameters_.sched_->getChooseTargetOption() == 
       SchedulerParameters::CHOOSE_TARGET_COMMENSAL)
   {
      SseArchive::SystemLog() 
         << "Reading tscope primary beam coordinates..." << endl;

      double ra2000Hours(-1);
      double dec2000Deg(-1);
      getTscopePrimaryBeamCoords(
         nssComponentTree->getTscopes(), &ra2000Hours, &dec2000Deg);

      SseArchive::SystemLog() 
         << "Primary Beam: RaHours: " << ra2000Hours 
         << " DecDeg: " << dec2000Deg << endl;
      
      nssParameters_.act_->setPrimaryBeamRaHours(ra2000Hours);
      nssParameters_.act_->setPrimaryBeamDecDeg(dec2000Deg);
   }

   if (followupEnabled_ && followup_->followupIsPending())
   {
      Activity *act(prepareValidFollowupActivity(nssComponentTree));
      if (act)
      {
         return act;
      }

      // Couldn't do followup, but no serious errors were thrown
      // so fall through and try to create a regular activity.
   }

   if (isCommensalCalPending())
   {
      commensalCalPending_ = false;

      SseArchive::SystemLog() << "Creating commensal cal activity..." << endl;

      return getCommensalCalActivity(nssComponentTree);
   }

   return getNonFollowupActivity(nssComponentTree);
}


void ObsActStrategy::setActStatusToDataCollComplete(Activity *activity)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(actStatusMutex_);

   ActivityMap::iterator index = actStatus_.find(activity);
   if (index == actStatus_.end())
   {
      throw SseException("Activity not found\n", __FILE__,
			 __LINE__, SSE_MSG_EXCEPTION, SEVERITY_ERROR);
   }
   else 
   {
      (*index).second->status_ = ActivityData::ACTIVITY_DATA_COLLECTION_COMPLETE;
   }
}

void ObsActStrategy::dataCollectionCompleteInternalHook(Activity *activity)
{
   VERBOSE2(getVerboseLevel(), 
	    "ObsActStrategy::dataCollectionCompleteInternal()" << endl;);

   ActivityId_t actId(activity->getId());
   updateObservedFrequencies(actId);

   setActStatusToDataCollComplete(activity);
}


void ObsActStrategy::activityCompleteInternalHook(
   Activity *activity, 
   bool failed)
{
   // do nothing
}

void ObsActStrategy::foundConfirmedCandidatesInternal(Activity *activity)
{
   if (followupEnabled_)
   {
      followup_->addActivityId(activity->getId());
   }
}

void ObsActStrategy::startNextActivityHook(Activity *activity)
{
   insertActStatus(activity);
}

void ObsActStrategy::insertActStatus(
   Activity *activity)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(actStatusMutex_);  

   ActivityData *activityData =
      new ActivityData(ActivityData::ACTIVITY_NOT_STARTED);

   actStatus_.insert(ActivityMap::value_type(activity, activityData));
}

void ObsActStrategy::cleanUpActivityHook(Activity *activity)
{
   eraseActStatus(activity);
}

void ObsActStrategy::eraseActStatus(Activity *activity)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(actStatusMutex_);

   ActivityMap::iterator index = actStatus_.find(activity);
   if (index == actStatus_.end())
   {
      throw SseException("Activity not found\n", __FILE__,
			 __LINE__, SSE_MSG_EXCEPTION, SEVERITY_ERROR);
   }
   else
   {
      ActivityData *actData = (*index).second;
      delete actData;

      actStatus_.erase(index);
   }

}


bool ObsActStrategy::readyToDoFollowup() const
{
   bool ready = followupEnabled_ && followupModeIsAuto_ 
      && followup_->followupIsPending();

   return ready;
}

bool ObsActStrategy::moreActivitiesToRun() 
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::moreActivitiesToRun()" << endl;);
   Assert(tuneDxs_);

   bool more = readyToDoFollowup() || 
      (!isWrapUpRequested() && tuneDxs_->moreActivitiesToRun());

   return more;
}

void ObsActStrategy::repeatStrategyHook()
{
   delete tuneDxs_;
   delete tuneZxs_;
   tuneDxs_ = getTuneDxs(getVerboseLevel());
   tuneZxs_ = getTuneZxs(getVerboseLevel());
}


bool ObsActStrategy::okToStartNewActivity()
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::okToStartNewActivity()" << endl;);

   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(actStatusMutex_);

   if (nssParameters_.sched_->pipeliningEnabled())
   {
      // there can be only one active activity, and it must be out of data collection
      if (actStatus_.size() > 1)
      {
	 return false;
      }

      for (ActivityMap::const_iterator index =
	      actStatus_.begin(); index != actStatus_.end(); ++index)
      {
	 if ((*index).second->status_ != ActivityData::ACTIVITY_DATA_COLLECTION_COMPLETE)
	 {
	    return false;
	 }
      }

      return true;
   }
   else  // not pipelining
   {
      // Only start an activity if no activities are currently running
      return actStatus_.size() == 0;
   }
    
}


TuneDxs* ObsActStrategy::getTuneDxs(int verboseLevel) const
{
   switch (nssParameters_.sched_->getTuneDxsOption())
   {
   case SchedulerParameters::TUNE_DXS_RANGE:

	   //cout.precision(12);
	 //cout << "beginfreq " << nssParameters_.sched_->getBeginObsFreqMhz()
      	//	<< " endfreq " << nssParameters_.sched_->getEndObsFreqMhz()
	//	<< endl;
	return new TuneDxsRangeCenter(
	 verboseLevel,
	 Range(nssParameters_.sched_->getBeginObsFreqMhz(),
	       nssParameters_.sched_->getEndObsFreqMhz()));
	
      //return new TuneDxsRangeCenterRound(
	 //verboseLevel,
	 //Range(nssParameters_.sched_->getBeginObsFreqMhz(),
	       //nssParameters_.sched_->getEndObsFreqMhz()),
	 //nssParameters_.sched_->getDxRound(),
	 //nssParameters_.sched_->getDxOverlap(),
	 //nssParameters_.sched_->getDxTuningTolerance());
	
   case SchedulerParameters::TUNE_DXS_USER:

      return new TuneDxsUser(verboseLevel);

   case SchedulerParameters::TUNE_DXS_FOREVER:

      return new TuneDxsForever(verboseLevel);

   default:

      AssertMsg(0, "Incorrect value for getTuneDxs");

   }
}

TuneZxs* ObsActStrategy::getTuneZxs(int verboseLevel) const
{
   switch (nssParameters_.sched_->getTuneDxsOption())
   {
   case SchedulerParameters::TUNE_DXS_RANGE:

	   //cout.precision(12);
	 //cout << "beginfreq " << nssParameters_.sched_->getBeginObsFreqMhz()
      	//	<< " endfreq " << nssParameters_.sched_->getEndObsFreqMhz()
	//	<< endl;
	return new TuneZxsRange(
	 verboseLevel,
	 Range(nssParameters_.sched_->getBeginObsFreqMhz(),
	       nssParameters_.sched_->getEndObsFreqMhz()));
	
	
   case SchedulerParameters::TUNE_DXS_USER:

      return new TuneZxsUser(verboseLevel);

   case SchedulerParameters::TUNE_DXS_FOREVER:

      return new TuneZxsForever(verboseLevel);

   default:

      AssertMsg(0, "Incorrect value for getTuneZxs");

   }
}

vector<TargetMerit::MeritFactor> ObsActStrategy::prepareTargetMeritFactors()
{
   string commaSepMeritNames(nssParameters_.sched_->getTargetMeritNames());
   SseArchive::SystemLog() << "Target merit factors: " << commaSepMeritNames << endl;

   string delimiters(",");
   vector<string> meritNames = SseUtil::tokenize(commaSepMeritNames, delimiters);

   vector<TargetMerit::MeritFactor> meritFactors;
   for (unsigned int i=0; i<meritNames.size(); ++i)
   {
      meritFactors.push_back(TargetMerit::nameToMeritFactor(meritNames[i]));
   }

   return meritFactors;
}

void ObsActStrategy::prepareOrderedTargets(
   float64_t bandwidthOfSmallestDxMhz,
   float64_t minAcceptableRemainingBandMhz)
{
   VERBOSE2(getVerboseLevel(), "ObsActStrategy::prepareOrderedTargets" 
            << endl;);

   Range freqRangeLimitsMhz(nssParameters_.sched_->getBeginObsFreqMhz(),
                            nssParameters_.sched_->getEndObsFreqMhz());
	 
   PermRfiMask permRfiMask(PermRfiMaskFilename, "permRfiMask", 
                           getVerboseLevel());
	 
   vector<FrequencyBand> permRfiBands(permRfiMask.getFreqBands());

   if (nssParameters_.sched_->getDecLowerLimitDeg() >
       nssParameters_.sched_->getDecUpperLimitDeg())
   {
      throw SseException( 
         "dec lower limit > dec upper limit in scheduler parameters.\n",
         __FILE__, __LINE__, SSE_MSG_INVALID_PARMS, SEVERITY_ERROR);
   }

   double primaryBeamsizeAtOneGhzArcSec = 
      nssParameters_.tscope_->getPrimaryFovAtOneGhzDeg() * AtaInformation::ArcSecPerDeg;
         
   double synthBeamsizeAtOneGhzArcSec =
      nssParameters_.tscope_->getBeamsizeAtOneGhzArcSec();

   bool autorise(false);
   if (nssParameters_.sched_->getChooseTargetOption() == 
       SchedulerParameters::CHOOSE_TARGET_AUTO_RISE)
   {
      autorise = true;
   }

   orderedTargets_ = new OrderedTargets(
      nssParameters_.db_->getDb(),
      getVerboseLevel(),
      nssParameters_.sched_->getMinNumberReservedFollowupObs(),
      nssParameters_.tscope_->getSiteLongWestDeg(),
      nssParameters_.tscope_->getSiteLatNorthDeg(),
      nssParameters_.tscope_->getSiteHorizDeg(),
      bandwidthOfSmallestDxMhz,
      minAcceptableRemainingBandMhz,
      nssParameters_.sched_->getBeamBandwidthMhz(),
      autorise,
      nssParameters_.dx_->getDataCollectionLengthSecs(),
      nssParameters_.sched_->getMaxTargetDistLightYears(),
      nssParameters_.sched_->getSunAvoidAngleDeg(),
      nssParameters_.sched_->getMoonAvoidAngleDeg(),
      nssParameters_.sched_->getGeosatAvoidAngleDeg(),
      nssParameters_.sched_->getZenithAvoidAngleDeg(),
      nssParameters_.sched_->getAutoRiseTimeCutoffMinutes(),
      nssParameters_.sched_->getWaitTargetComplete(),
      nssParameters_.sched_->getDecLowerLimitDeg(),
      nssParameters_.sched_->getDecUpperLimitDeg(),
      nssParameters_.sched_->getPrimaryTargetIdCountCutoff(),
      nssParameters_.sched_->getHighPriorityCatalogMaxCounts(),
      nssParameters_.sched_->getHighPriorityCatalogNames(),
      nssParameters_.sched_->getLowPriorityCatalogNames(),
      prepareTargetMeritFactors(),
      freqRangeLimitsMhz,
      permRfiBands,
      primaryBeamsizeAtOneGhzArcSec,
      synthBeamsizeAtOneGhzArcSec
      );
}

/*
  Choose the first target automatically from the database,
  and return its target id.
  Also determine the primary beam FOV, and return a 
  target id for that too.  Note that this may be a set
  to a dummy value if there isn't a corresponding id
  (e.g., if the primary position is set using coordinates
  instead of a target id).
  Return additional targets as requested.
*/

void ObsActStrategy::chooseAutoTargetsFromDb(
   int nTargetsToChoose,
   double bandwidthOfSmallestDxMhz,
   double minAcceptableRemainingBandMhz,
   double minTargetSepInBeams,
   bool areActsRunning,
   TargetId & firstTargetId,    // returned
   ObsRange & chosenObsRange,   // returned
   TargetId & primaryTargetId,  // returned
   TargetIdSet & additionalTargetIds)  // returned
{
   VERBOSE2(getVerboseLevel(), 
            "ObsActStrategy::chooseFirstAutoTargetFromDb()" << endl;);

   if (! nssParameters_.db_->useDb())
   {
      throw SseException( 
	 "automatic target choice does not work when database is off\n",  
	 __FILE__, __LINE__, SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
   }
  
   try {

      if (! orderedTargets_) 
      {
         prepareOrderedTargets(bandwidthOfSmallestDxMhz,
                               minAcceptableRemainingBandMhz);
      }

      SseArchive::SystemLog() << "Choosing first target..." << endl;

      /*
        Determine who chooses primary FOV and how.
       */
      if (nssParameters_.sched_->getChooseTargetOption() == 
          SchedulerParameters::CHOOSE_TARGET_COMMENSAL)
      {
         // Assumes current tscope primary beam coordinates
         // have already been loaded into the activity parameters

         nssParameters_.act_->setPointPrimaryBeam(false);

         nssParameters_.act_->setPrimaryBeamPositionType(
            ActParameters::PRIMARY_BEAM_POS_COORDS);

         double ra2000Rads = SseAstro::hoursToRadians(
            nssParameters_.act_->getPrimaryBeamRaHours());
         
         double dec2000Rads = SseAstro::degreesToRadians(
            nssParameters_.act_->getPrimaryBeamDecDeg());

         orderedTargets_->useThisPrimaryFovCenter(
            ra2000Rads, dec2000Rads);
      }
      else
      {
         // We're the primary observer
         nssParameters_.act_->setPointPrimaryBeam(true);

         if (nssParameters_.sched_->getChooseTargetOption() == 
             SchedulerParameters::CHOOSE_TARGET_SEMIAUTO)
         {
            // Use primary pointing coords specified by user.

            // TBD: make this a warning?
            if (nssParameters_.act_->getPrimaryBeamPositionType() != 
                ActParameters::PRIMARY_BEAM_POS_COORDS)
            {
               throw SseException( 
                  string("Can't set primary beam FOV by target id in semiauto ")
                  + string("scheduling mode, must be by RA/Dec coords only\n"),
                  __FILE__, __LINE__, SSE_MSG_INVALID_PARMS, SEVERITY_ERROR);
            }
            
            double ra2000Rads = SseAstro::hoursToRadians(
               nssParameters_.act_->getPrimaryBeamRaHours());
         
            double dec2000Rads = SseAstro::degreesToRadians(
               nssParameters_.act_->getPrimaryBeamDecDeg());
            
            orderedTargets_->useThisPrimaryFovCenter(
               ra2000Rads, dec2000Rads);
            
         }
         else
         {
            // "auto/autorise" modes use target IDs exclusively

            nssParameters_.act_->setPrimaryBeamPositionType(
               ActParameters::PRIMARY_BEAM_POS_TARGET_ID);

            if (isRotatePrimaryTargetIdsPending())
            {
               rotatePrimaryTargetIdsPending_ = false;
               
               SseArchive::SystemLog() << "Rotating primary target IDs..." << endl;
               
               orderedTargets_->rotatePrimaryTargetIds();
            }
         }
      }

      time_t currentTime;
      time(&currentTime);

      Assert(orderedTargets_);
      orderedTargets_->chooseTargets(
         nTargetsToChoose, currentTime, 
	 nssParameters_.sched_->getMinTargetSepBeamsizes(),
         areActsRunning, 
	 firstTargetId, chosenObsRange, 
         primaryTargetId, additionalTargetIds);

      VERBOSE2(getVerboseLevel(), 
	       "firstTargetId: " << firstTargetId
	       << " primaryTargetId: " << primaryTargetId
	       << " Unobserved frequencies are :  " <<
	       chosenObsRange << endl;);
    
   }
   catch (ActivityStartWaitingForTargetComplete &except)
   {
      throw except;
   }
   catch (SseException &except)
   {
      except.newDescription(string(except.descrip()) + 
				   "Automatic Target Choice failed.\n");
      except.newCode(SSE_MSG_AUTO_TARG_FAILED);
      throw except;
   }
}

void ObsActStrategy::strategyCleanupHook()
{
   // clear any pending followups
   followup_->clearActivityIdList();

   // cancel any pending timers
   commensalCalTimer_.cancelTimer();
   rotatePrimaryTargetIdsTimer_.cancelTimer();
}

float64_t ObsActStrategy::computeChanCenterFreq( DxList &dxList, int32_t outputChannels,
		float64_t mhzPerChannel) 
{
           int32_t dcChannel = outputChannels/2;
             DxProxy *firstDxProxy = *dxList.begin();
            double firstDxFreq = firstDxProxy->getDxSkyFreq();
            int32_t firstDxChan = firstDxProxy->getChannelNumber();
            float64_t channelizerTuneFreqMhz = (float64_t)(dcChannel - firstDxChan)*mhzPerChannel + firstDxFreq;
//            cout << " Channelizer Center Freq " << channelizerTuneFreqMhz << endl;

            return channelizerTuneFreqMhz;
}
