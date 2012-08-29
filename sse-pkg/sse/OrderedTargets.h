/*******************************************************************************

 File:    OrderedTargets.h
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

/*****************************************************************
 * OrderedTargets.h - declaration of functions defined in OrderedTargets.h
 * These classes support ordering targets to be observed
 * PURPOSE:  
 *****************************************************************/

#ifndef ORDEREDTARGETS_H
#define ORDEREDTARGETS_H

#include "Target.h"
#include "SseTimer.h"
#include "TargetId.h"
#include "TargetIdSet.h"
#include "ActivityId.h"
#include "TargetMerit.h"
#include "mysql.h"
#include <set>
#include <sstream>

using std::map;
using std::set;
using std::list;
using std::stringstream;

class TargetMerit;
class SiteView;

class OrderedTargets
{
public:
  OrderedTargets(MYSQL* db, 
                 int verboseLevel,
		 int minNumberReservedFollowupObs,
                 double siteLongWestDeg,
                 double siteLatNorthDeg,
                 double siteHorizDeg,
		 double bandwidthOfSmallestDxMhz,
		 double minAcceptableRemainingBandMhz,
                 double maxDxTuningSpreadMhz,
		 bool autorise,  // if true, choose target near rising
		 double obsLengthSec,
		 double maxDistLightYears,
		 double sunAvoidAngleDeg,
		 double moonAvoidAngleDeg,
                 double geosatAvoidAngleDeg,
                 double zenithAvoidAngleDeg,
		 double autoRiseTimeCutoffMinutes,
		 bool waitTargetComplete,
		 double decLowerLimitDeg,
		 double decUpperLimitDeg,
                 int primaryTargetIdCountCutoff,
                 int highPriorityCatalogMaxCounts,
                 const string & highPriorityCatalogNames,
                 const string & lowPriorityCatalogNames,
                 const vector<TargetMerit::MeritFactor> & targetMeritFactors,
		 const Range& freqRangeLimitsMhz,
		 vector<FrequencyBand> & permRfiMaskFreqBands,
                 double primaryBeamsizeAtOneGhzArcSec,
                 double synthBeamsizeAtOneGhzArcSec
     );

  virtual ~OrderedTargets();

  virtual void useThisPrimaryFovCenter(double ra2000Rads, double dec2000Rads);

  virtual void retrieveObsHistFromDbForZxs(ObsRange &observedFreqs, TargetId target);
  virtual void chooseTargets(
     int nTargetsToChoose,
     time_t obsDate,
     double minTargetSeparationInBeams,  
     bool areActsRunning,
     TargetId & firstTargetId,  // returned target id
     ObsRange & chosenObsRange,  // returned freqs to be observed
     TargetId & primaryTargetId,  // returned
     TargetIdSet & additionalTargetIds  // returned target ids
     );

  virtual int getVerboseLevel() const;
  virtual int setVerboseLevel(int level);
  virtual double getMinRemainingTimeOnTargetRads() const;
  
  virtual void updateObservedFreqsForTargetsFromDbObsHistory(ActivityId_t actId);

  virtual void rotatePrimaryTargetIds();

  friend ostream &operator<<(ostream& os,
			     const OrderedTargets& orderedTargets);

  friend class TargetSort;

protected:

  typedef map<TargetId, Target *> TargetMap; 
  typedef map<string, int> NameCountMap;
  typedef map<TargetId, int> HistogramMap;

  virtual void chooseAdditionalTargets(
     int nTargetsToChoose,
     double minTargetSeparationBeamsizes,  
     TargetIdSet & chosenTargetIds  // returned target ids
     );

  virtual void resetCulledTargetMap();
  virtual void setObsDate(time_t obsDate);
  virtual void setObsDateOnTargets(TargetMap & targetMap);

  virtual void getObservedFreqsForActivity(
     TargetMap & targetMap, ActivityId_t activityId);

  // returns pointer to best observation
  virtual TargetMap::const_iterator
  	bestObsRise(TargetMap & targetMap);
  virtual TargetMap::const_iterator
   	bestObsRise(TargetMap & targetMap, TargetId currentTargetId);

  virtual void resetObservationsForCompletelyObservedTargets(
     TargetMap & targetMap);

  virtual void cullBySunAvoidAngle(TargetMap &targetMap);
  virtual void cullByMoonAvoidAngle(TargetMap &targetMap);
  virtual void cullByGeosatAvoidAngle(TargetMap &targetMap);
  virtual void cullByZenithAvoidAngle(TargetMap &targetMap);
  virtual void cullNotVisible(TargetMap &targetMap);
  virtual void cullByPartiallyBlockedPrimaryTargetRegions(
     TargetMap &targetMap);

  virtual void cullTooFarFromPrimaryFovCenter(TargetMap &targetMap,
                                              RaDec & primaryFovCenter);

  virtual void cullOutsidePrimaryFov(
     TargetMap &targetMap, const RaDec & centerRaDec,
     double primaryFovRads);

  virtual void cullByPrimaryTargetId(TargetMap &targetMap,
                                     TargetId primaryTargetIdToKeep);

  virtual void cullByMinTargetSeparation(
     TargetMap &targetMap, TargetId chosenTargetId,
     double minSepRads);

  virtual void prepareTargetMap(time_t obsDate);

  virtual void getBestTarget(
     TargetMap & targetMap,
     TargetId currentTargetId, // target currently being observed
     bool activitiesInProgress,
     TargetId & bestTargetId,    // return: chosen target
     ObsRange & bestTargetObsRange  // return: frequencies needed for this target
     );

  virtual void printTargetInfoInSystemlog(TargetId targetId,
					Target *target,
					const ObsRange &obsRange);
  virtual void eraseTarget(TargetMap::iterator it, TargetMap &targetMap);

  virtual TargetId determinePrimaryTargetId(TargetMap & targetMap,
                                            TargetId targetId);

  virtual void createPrimaryTargetIdHistogram(const TargetMap & targetMap,
                                              HistogramMap & histMap);

  virtual void loadPrimaryTargetIdRotation(const TargetMap & targetMap);



  virtual void updatePreferredPrimaryTargetIdInTargets();

  virtual void validateHighAndLowPriorityCatalogNames();

  virtual void assignCatalogs(const string &requestedCatalogs,
                              const NameCountMap & catalogNameCounts,
                              int maxCounts,
                              vector<string> & assignedCatalogs);
  
  virtual void getCatalogNameCounts(NameCountMap & nameCountMap);

  virtual void lookupCatalogPriority(const string & catalog,
                                     Target::CatalogPriority & priority,
                                     int & order);

  string prepareQueryRestrictionForCatalogs(vector<string> catalogNames);

  virtual void prepareHighPriorityTargets();
  virtual void loadHighPriorityTargets(TargetMap &targetMap);

  virtual void prepareOnDemandTargets();
  virtual void loadOnDemandTargets(TargetMap &targetMap);

  virtual void getTargets(TargetMap & targetMap, 
                  double decLowerLimitDeg, double decUpperLimitDeg,
                  const string &queryRestriction);

  virtual void retrieveObsHistFromDbForTargets(TargetMap & targetMap);

  virtual bool isPositionVisible(const RaDec & raDec);

  virtual void setPrimaryBeamCenter(const RaDec &requestedPosition);

 private:

  // these are called by constructor or destructor - keep nonvirtual
  double computeMinRemainingTimeOnTargetRads() const;

  void prepareDesiredObservingFreqRanges(
     const Range& freqRangeLimitsMhz,
     vector<FrequencyBand> & permRfiMaskFreqBands);

  void deleteTargets(TargetMap & targetMap);


  MYSQL* db_;
  double obsLengthSec_;
  TargetMerit *targetMerit_;
  SiteView *siteView_;
  double lmstRads_;		// local mean sidereal time
  TargetMap highPriorityTargetMap_; 
  TargetMap onDemandTargetMap_; 
  TargetMap culledTargetMap_; // culled list of targets
  int verboseLevel_;
  int minNumberReservedFollowupObs_; 
  double maxDistLightYears_;
  double bandwidthOfSmallestDxMhz_;
  double minAcceptableRemainingBandMhz_;
  double maxDxTuningSpreadMhz_;
  double sunAvoidAngleDeg_;
  double moonAvoidAngleDeg_;
  double geosatAvoidAngleDeg_;
  double zenithAvoidAngleDeg_;
  double autoRiseTimeCutoffMinutes_;
  bool waitTargetComplete_;
  double decLowerLimitDeg_;
  double decUpperLimitDeg_;
  int primaryTargetIdCountCutoff_;
  int highPriorityCatalogsMaxCounts_;
  string requestedHighPriorityCatalogs_;
  string requestedLowPriorityCatalogs_;
  Range freqRangeLimitsMhz_;

  time_t obsDate_;
  bool autorise_;
  TargetId firstChosenTargetId_;
  double primaryBeamsizeAtOneGhzArcSec_;
  double synthBeamsizeAtOneGhzArcSec_;
  double minRemainingTimeOnTargetRads_;
  double observerLatRad_;

  TargetIdSet currentSecondaryTargetsInUse_;
  bool usePreselectedPrimaryFovCenter_;
  bool onDemandTargetMapUpToDate_;
  ObsRange desiredObservingFreqRangesMhz_;
  RaDec primaryFovCenterRaDec2000_;
  stringstream selectionLogStrm_;
  vector<string> highPriorityCatalogs_;
  vector<string> lowPriorityCatalogs_;
  vector<TargetId> primaryTargetIdRotation_;
};



// sort target by dreher merit, at given lmst
class TargetSort
{
 public:
  TargetSort();
  bool operator()(const OrderedTargets::TargetMap::const_iterator &first,
		  const OrderedTargets::TargetMap::const_iterator &second) const;
 private:
};



#endif /* ORDEREDTARGETS_H */
