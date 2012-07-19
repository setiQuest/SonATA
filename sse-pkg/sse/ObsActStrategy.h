/*******************************************************************************

 File:    ObsActStrategy.h
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

/*
 Observation Activity Strategy
 Controls activities that perform observations.
*/

#ifndef ObsActStrategy_H
#define ObsActStrategy_H

#include "ActivityStrategy.h"
#include "CalTargets.h"
#include "DxList.h"
#include "Timeout.h"
#include "TargetMerit.h"
#include <map>

class Scheduler;
class Site;
class NssComponentTree;
class NssParameters;
class TuneDxs;
class TuneZxs;
class Followup;
class OrderedTargets;
class ActivityData;
class ObsRange;

class ObsActStrategy : public ActivityStrategy
{
 public:
   ObsActStrategy(Scheduler *scheduler,
                  Site *site,
                  const NssParameters &nssParameters,
                  int verboseLevel);
   virtual ~ObsActStrategy();

 protected:

   typedef std::map<string, DxList> DxListByBeamMap;  // key is beamName

   // base class overrides
   virtual Activity *getNextActivity(NssComponentTree *nssComponentTree);
   virtual bool moreActivitiesToRun();
   virtual bool okToStartNewActivity();

   // overridden hook methods
   virtual void startInternalHook();
   virtual void setVerboseLevelInternalHook(int verboseLevel);
   virtual void dataCollectionCompleteInternalHook(Activity *activity);
   virtual void activityCompleteInternalHook(Activity *activity, bool failed);
   virtual void startNextActivityHook(Activity *activity);
   virtual void cleanUpActivityHook(Activity *activity);
   virtual void repeatStrategyHook();
   virtual void strategyCleanupHook();

   virtual Activity *getFollowupActivity(const NssParameters &nssParams,
				 NssComponentTree *nssComponentTree);
   virtual Activity *getNonFollowupActivity(NssComponentTree *nssComponentTree);
   virtual Activity *getCommensalCalActivity(NssComponentTree *nssComponentTree);
   virtual Activity *prepareValidFollowupActivity(NssComponentTree *nssComponentTree);

   virtual bool readyToDoFollowup() const;
   virtual void foundConfirmedCandidatesInternal(Activity *activity);

   virtual vector<TargetMerit::MeritFactor> prepareTargetMeritFactors();
   virtual void prepareOrderedTargets(double bandwidthOfSmallestDxMhz,
                                      double minAcceptableRemainingBandMhz);
   
   virtual void chooseTargets(const string & firstTargetBeamName, 
                              const DxList &dxList);

   virtual void assignAdditionalTargetsToBeams(const string & firstChosenBeamName,
                                               const TargetIdSet & additionalTargetIds);

   virtual void tuneDxsForMultipleBeamsOnSingleTarget(NssComponentTree *nssComponentTree);

   virtual void pickMultipleTargetsAndTuneDxs(NssComponentTree *nssComponentTree);

   virtual void pickSingleTargetAndTuneDxs(NssComponentTree *nssComponentTree);

   virtual void channelizeDxTunings(DxList & dxList, double chanzerTuneMhz,
                                     double chanzerWidthMhz);

   virtual bool isTargetTooCloseToSetting(time_t currentTime, time_t setTime);
   virtual TuneDxs * getTuneDxs(int verboseLevel) const;
   virtual TuneZxs * getTuneZxs(int verboseLevel) const;

   virtual void getDxListForEachBeam(
      NssComponentTree *nssComponentTree, 
      vector<string> & requestedBeamNames, 
      DxListByBeamMap & dxListByBeamMap);

   virtual void getZxListForEachBeam(
      NssComponentTree *nssComponentTree, 
      vector<string> & requestedBeamNames, 
      DxListByBeamMap & zxListByBeamMap);

   virtual DxList getShortestDxList(DxListByBeamMap & dxListByBeamMap,
			      string & shortestListBeamName);

   virtual void copyDxTuningsFromOneBeamToTheOthers(
      DxListByBeamMap & dxListByBeamMap,
      const string & sourceBeamName);

   virtual void copyZxTuningsFromOneBeamToTheOthers(
      DxListByBeamMap & zxListByBeamMap,
      const string & sourceBeamName);

   virtual void chooseAutoTargetsFromDb(
      int nTargetsToChoose,
      double bandwidthOfSmallestDxMhz,
      double minAcceptableRemainingBandMhz,
      double minTargetSepInBeams,
      bool areActsRunning,
      TargetId & firstTargetId,    
      ObsRange & chosenObsRange,  
      TargetId & primaryTargetId,
      TargetIdSet & additionalTargetIds);

   virtual void configureCommensalCalTimer();
   virtual void requestCommensalCal();
   virtual bool isCommensalCalPending();
   virtual TargetId chooseCommensalCalTargetId();

   virtual void configureRotatePrimaryTargetIdsTimer();
   virtual void requestRotatePrimaryTargetIds();
   virtual bool isRotatePrimaryTargetIdsPending();

   virtual void updateObservedFrequencies(ActivityId_t actId);
   virtual void setActStatusToDataCollComplete(Activity *activity);
   virtual void insertActStatus(Activity *activity);
   virtual void eraseActStatus(Activity *activity);
   virtual float64_t computeChanCenterFreq( DxList &dxList, 
		int32_t outputChannels, float64_t mhzPerChannel);

 private:

   typedef std::map<Activity*, ActivityData*> ActivityMap;

   NssParameters nssParameters_;
   TuneDxs *tuneDxs_;
   TuneZxs *tuneZxs_;
   Followup *followup_;
   Scheduler *scheduler_;
   bool followupEnabled_;
   bool followupModeIsAuto_;
   ActivityMap actStatus_;
   OrderedTargets *orderedTargets_;
   Timeout<ObsActStrategy> commensalCalTimer_;
   ACE_Atomic_Op<ACE_Token, bool> commensalCalPending_;
   Timeout<ObsActStrategy> rotatePrimaryTargetIdsTimer_;
   ACE_Atomic_Op<ACE_Token, bool> rotatePrimaryTargetIdsPending_;
   CalTargets calTargets_;
   mutable ACE_Recursive_Thread_Mutex actStatusMutex_;

   // disable copy construction & assignment.
   // don't define these
   ObsActStrategy(const ObsActStrategy& rhs);
   ObsActStrategy& operator=(const ObsActStrategy& rhs);
  
};

#endif 
