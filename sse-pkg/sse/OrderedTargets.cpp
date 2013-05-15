/*******************************************************************************

 File:    OrderedTargets.cpp
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
  Creates a list of SETI targets ordered & filtered by various
  selection criteria.

  Several targetMaps are used to organized the targets. They
  are:

    highPriorityTargetMap
    onDemandTargetMap
    culledTargetMap

  If we're choosing the primary beam FOV, then the highPriorityTargetMap
  contains preferred targets, and is always kept loaded.  The
  onDemandTargetMap is used to load additional low priority targets as
  needed to fill in the primary FOV.  They are dropped when the
  primary FOV changes.

  If we're using a preassigned primary beam FOV, then all targets
  (high and low priority) get loaded as needed into the
  onDemandTargetMap, and are deleted when the FOV changes.

  The culledTargetMap is used as a processing space to
  select targets for a particular activity.
*/

#include <ace/OS.h>
#include "OrderedTargets.h"
#include "SignalMask.h"
#include "ActivityException.h"
#include "SseArchive.h"
#include "ActParameters.h"
#include "MysqlQuery.h"
#include "AtaInformation.h"
#include "OffPositions.h"
#include "SseAstro.h"
#include "SseUtil.h"
#include "TargetMerit.h"
#include "DebugLog.h"
#include "Assert.h"
#include <algorithm>
#include <sstream>

using namespace std;

const int NoPrimaryTargetId = -1;

OrderedTargets::OrderedTargets(MYSQL* db,
			       int verboseLevel,
			       int minNumberReservedFollowupObs,
                               double siteLongWestDeg,
                               double siteLatNorthDeg,
                               double siteHorizDeg,
			       double bandwidthOfSmallestDxMhz,
			       double minAcceptableRemainingBandMhz,
                               double maxDxTuningSpreadMhz,
			       bool autorise,
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
                               double synthBeamsizeAtOneGhzArcSec):
   db_(db),
   obsLengthSec_(obsLengthSec),
   verboseLevel_(verboseLevel),
   minNumberReservedFollowupObs_(minNumberReservedFollowupObs),
   maxDistLightYears_(maxDistLightYears),
   bandwidthOfSmallestDxMhz_(bandwidthOfSmallestDxMhz),
   minAcceptableRemainingBandMhz_(minAcceptableRemainingBandMhz),
   maxDxTuningSpreadMhz_(maxDxTuningSpreadMhz),
   sunAvoidAngleDeg_(sunAvoidAngleDeg),
   moonAvoidAngleDeg_(moonAvoidAngleDeg),
   geosatAvoidAngleDeg_(geosatAvoidAngleDeg),
   zenithAvoidAngleDeg_(zenithAvoidAngleDeg),
   autoRiseTimeCutoffMinutes_(autoRiseTimeCutoffMinutes),
   waitTargetComplete_(waitTargetComplete),
   decLowerLimitDeg_(decLowerLimitDeg),
   decUpperLimitDeg_(decUpperLimitDeg),
   primaryTargetIdCountCutoff_(primaryTargetIdCountCutoff),
   highPriorityCatalogsMaxCounts_(highPriorityCatalogMaxCounts),
   requestedHighPriorityCatalogs_(highPriorityCatalogNames),
   requestedLowPriorityCatalogs_(lowPriorityCatalogNames),
   freqRangeLimitsMhz_(freqRangeLimitsMhz),
   obsDate_(0),
   autorise_(autorise),
   firstChosenTargetId_(0),
   primaryBeamsizeAtOneGhzArcSec_(primaryBeamsizeAtOneGhzArcSec),
   synthBeamsizeAtOneGhzArcSec_(synthBeamsizeAtOneGhzArcSec),
   usePreselectedPrimaryFovCenter_(false),
   onDemandTargetMapUpToDate_(false)
{
   targetMerit_ = new TargetMerit(targetMeritFactors);
   
   siteView_ = new SiteView(siteLongWestDeg,
                            siteLatNorthDeg,
                            siteHorizDeg);

   observerLatRad_ = siteView_->getLatRads();

   minRemainingTimeOnTargetRads_ = computeMinRemainingTimeOnTargetRads();

   prepareDesiredObservingFreqRanges(
      freqRangeLimitsMhz, permRfiMaskFreqBands);

   // default to impossible position to force an initial update
   primaryFovCenterRaDec2000_.ra = Radian(4*M_PI);
   primaryFovCenterRaDec2000_.dec = Radian(M_PI);

   validateHighAndLowPriorityCatalogNames();
}

OrderedTargets::~OrderedTargets()
{
   delete targetMerit_;
   delete siteView_;

   deleteTargets(highPriorityTargetMap_);
   deleteTargets(onDemandTargetMap_);
}

void OrderedTargets::deleteTargets(TargetMap & targetMap)
{
   for (TargetMap::iterator it = targetMap.begin();
	it != targetMap.end(); ++it)
   {
      delete (*it).second;
   }
   targetMap.clear();
}

bool OrderedTargets::isPositionVisible(const RaDec & raDec)
{
   double timeSinceRiseRads;
   double timeUntilSetRads;

   bool visible = siteView_->isTargetVisible(
      lmstRads_, raDec, timeSinceRiseRads, timeUntilSetRads);

   return visible;
}

#if 0
static void printVector(const string & name, const vector<string> values)
{
   cout << name << endl;
   for (unsigned int i = 0; i<values.size(); ++i)
   {
      cout << values[i] << endl;
   }
}
#endif

/*
  Validate the requested target catalog names against the database.
 */
void OrderedTargets::validateHighAndLowPriorityCatalogNames()
{
   string methodName("OrderedTargets::validateHighAndLowPriorityCatalogNames: ");

   VERBOSE2(getVerboseLevel(), methodName << endl;);    
   SseArchive::SystemLog() << "Validating requested high & low priority catalog names..." << endl;

   NameCountMap catalogNameCounts;
   getCatalogNameCounts(catalogNameCounts);

   assignCatalogs(requestedHighPriorityCatalogs_, catalogNameCounts, 
                  highPriorityCatalogsMaxCounts_, highPriorityCatalogs_);

   int lowPriorityCatalogsMaxCounts(-1);
   assignCatalogs(requestedLowPriorityCatalogs_, catalogNameCounts, 
                  lowPriorityCatalogsMaxCounts, lowPriorityCatalogs_);

#if 0
   printVector("----high: ", highPriorityCatalogs_);
   printVector("----low: ", lowPriorityCatalogs_);
#endif

}

/*
  Verify requested catalogs and assign them to the
  assignedCatalogs vector.

  requestedCatalogs:  comma separated list of catalog names
  catalogNameCounts:  map of catalog counts indexed by catalog name
  maxCounts:  maximum accepted size of catalog (ignored if <= 0)
  assignedCatalogs: destination vector

  Throws an exception if catalog not found or if exceeds maxCounts.
 */

void OrderedTargets::assignCatalogs(const string &requestedCatalogs,
                                    const NameCountMap & catalogNameCounts,
                                    int maxCounts,
                                    vector<string> & assignedCatalogs)
{
   string delimiters(" ,");
   vector<string> requestedCatalogsVect = 
      SseUtil::tokenize(requestedCatalogs, delimiters);

   if (requestedCatalogsVect.empty())
   {
      throw SseException("empty high or low priority catalog list specified\n",
                         __FILE__,  __LINE__, SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
   }
   
   for (unsigned int i=0; i<requestedCatalogsVect.size(); ++i)
   {
      NameCountMap::const_iterator it =
         catalogNameCounts.find(requestedCatalogsVect[i]);
      if (it != catalogNameCounts.end())
      {
         int counts(it->second);
         if (maxCounts > 0 && counts > maxCounts)
         {
            throw SseException("size of requested catalog '" + requestedCatalogsVect[i]
                               +"' exceeds max limit of " +
                               SseUtil::intToStr(maxCounts) + "\n",
                               __FILE__,  __LINE__, SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
         }
         assignedCatalogs.push_back(requestedCatalogsVect[i]);
      }
      else
      {
         throw SseException("requested catalog '" + requestedCatalogsVect[i]
                            + "' not found in database\n",
                             __FILE__,  __LINE__, SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }
   }

}

/*
  For the given catalog, determine whether it's on the
  high or low priority list, and find the order in which
  it appears on that list.  The order is zero based.
 */

void OrderedTargets::lookupCatalogPriority(const string & catalog,
                                           Target::CatalogPriority & priority,
                                           int & order)
{
   vector<string>::const_iterator itHigh = 
      find(highPriorityCatalogs_.begin(), highPriorityCatalogs_.end(), catalog);
   if (itHigh != highPriorityCatalogs_.end())
   {
      priority = Target::CatalogHighPriority;
      order = itHigh - highPriorityCatalogs_.begin();

      return;
   }

   vector<string>::const_iterator itLow = 
      find(lowPriorityCatalogs_.begin(), lowPriorityCatalogs_.end(), catalog);
   if (itLow != lowPriorityCatalogs_.end())
   {
      priority = Target::CatalogLowPriority;
      order = itLow - lowPriorityCatalogs_.begin();

      return;
   }
   
   throw SseException("lookupCatalogPriority:: failed to find catalog '"
                      + catalog + "'\n",
                      __FILE__,  __LINE__, SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);

}



/*
  Look up target counts for all catalogs that have targets with autoschedule
  set to 'Yes'. 

 */
void OrderedTargets::getCatalogNameCounts(NameCountMap & nameCountMap)
{
   string methodName("OrderedTargets::getCatalogNameCounts: ");

   VERBOSE2(getVerboseLevel(), methodName << endl;);    
   SseArchive::SystemLog() << "Getting target catalog names & counts from database..." << endl;

   stringstream targetCatQuery;
   targetCatQuery << "select catalog, count(*) as count from TargetCat "
                  << "where autoSchedule = 'Yes' group by catalog";

   enum resultCols { catalogCol, countsCol, numCols };

   MysqlQuery query(db_);
   query.execute(targetCatQuery.str(), numCols, __FILE__, __LINE__);
  
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      string catalog(query.getString(row, catalogCol,
                                     __FILE__, __LINE__));

      int counts(query.getInt(row, countsCol, 
                              __FILE__, __LINE__));

      nameCountMap[catalog] = counts;
   }
}

void OrderedTargets::loadHighPriorityTargets(TargetMap & targetMap)
{
   const string methodName("OrderedTargets::loadHighPriorityTargets: ");
   VERBOSE2(getVerboseLevel(), methodName << endl;);
   SseArchive::SystemLog() << "Loading high priority targets from database..." << endl;

   getTargets(targetMap,
              decLowerLimitDeg_, decUpperLimitDeg_,
              prepareQueryRestrictionForCatalogs(highPriorityCatalogs_));
}

string OrderedTargets::prepareQueryRestrictionForCatalogs(vector<string> catalogNames)
{
   stringstream queryRestrict;
   queryRestrict << " and (";
   for (unsigned int i=0; i< catalogNames.size(); ++i)
   {
      queryRestrict << "catalog = '" << catalogNames[i] << "' or ";
   }
   queryRestrict << "false) ";

   return queryRestrict.str();
}

void OrderedTargets::prepareHighPriorityTargets()
{
   // Only load it once
   if (highPriorityTargetMap_.empty())
   {
      loadHighPriorityTargets(highPriorityTargetMap_);

      loadPrimaryTargetIdRotation(highPriorityTargetMap_);
      
      updatePreferredPrimaryTargetIdInTargets();

      retrieveObsHistFromDbForTargets(highPriorityTargetMap_);
   }
}

/*
  Prepare the rotation list of primary target Ids.
  Shuffle it so that it's not the same every time observing starts.
*/
void OrderedTargets::loadPrimaryTargetIdRotation(const TargetMap & targetMap)
{
   HistogramMap histMap;
   createPrimaryTargetIdHistogram(targetMap, histMap);
   
   HistogramMap::const_iterator it;
   for (it = histMap.begin(); it != histMap.end(); ++it)
   {
      if (it->first != NoPrimaryTargetId)
      {
         primaryTargetIdRotation_.push_back(it->first);
      }
   }

   random_shuffle(primaryTargetIdRotation_.begin(), primaryTargetIdRotation_.end());
}

void OrderedTargets::rotatePrimaryTargetIds()
{
   /* move the front value to the end */
   if (! primaryTargetIdRotation_.empty())
   {
      rotate(primaryTargetIdRotation_.begin(), 
             primaryTargetIdRotation_.begin() + 1,
             primaryTargetIdRotation_.end());

      updatePreferredPrimaryTargetIdInTargets();
   }
}

void OrderedTargets::updatePreferredPrimaryTargetIdInTargets()
{
   if (! primaryTargetIdRotation_.empty())
   {
      TargetId preferredPrimaryId(primaryTargetIdRotation_.front());

      VERBOSE2(getVerboseLevel(), "OrderedTargets:: preferred primary target id: " 
               << preferredPrimaryId << endl;);

      TargetMap & targetMap(highPriorityTargetMap_);
      for (TargetMap::iterator it = targetMap.begin();
           it != targetMap.end(); ++it)
      {
         Target *target = (*it).second;
         target->setPreferredPrimaryTargetId(preferredPrimaryId);
      }
   }
}

void OrderedTargets::prepareOnDemandTargets()
{
   const string methodName("prepareOnDemandTargets: ");

   // Only do this once per primary FOV change
   
   if (! onDemandTargetMapUpToDate_)
   {
      VERBOSE2(getVerboseLevel(), methodName << endl;);

      deleteTargets(onDemandTargetMap_);
      
      loadOnDemandTargets(onDemandTargetMap_);
      
      retrieveObsHistFromDbForTargets(onDemandTargetMap_);

      onDemandTargetMapUpToDate_ = true;
   }

}

void OrderedTargets::loadOnDemandTargets(TargetMap &targetMap)
{
   const string methodName("loadOnDemandTargets: ");
   VERBOSE2(getVerboseLevel(), methodName << endl;);

   /*
     Get targets that fall roughly in or around the 
     current primary FOV based on the lowest possible
     observing frequency.
   */

   double beamCenterRaHours = SseAstro::radiansToHours(
      primaryFovCenterRaDec2000_.ra.getRadian());

   double beamCenterDecDeg = SseAstro::radiansToDegrees(
      primaryFovCenterRaDec2000_.dec.getRadian());

   VERBOSE2(getVerboseLevel(), methodName 
            << " Primary beam center RA hours: " 
            << beamCenterRaHours 
            << " Dec deg: " << beamCenterDecDeg
            << endl;);

   // Get the max primary beamsize in degrees,
   // use it to set upper & lower dec limits.

   double maxPrimaryBeamsizeRads = AtaInformation::ataBeamsizeRadians(
      freqRangeLimitsMhz_.low_, primaryBeamsizeAtOneGhzArcSec_);

   double maxPrimaryBeamsizeDeg(SseAstro::radiansToDegrees(
      maxPrimaryBeamsizeRads));

   // Note: North Pole crossing should be fine.
   double decLowerLimitDeg(beamCenterDecDeg - maxPrimaryBeamsizeDeg/2);
   double decUpperLimitDeg(beamCenterDecDeg + maxPrimaryBeamsizeDeg/2);

   VERBOSE2(getVerboseLevel(), methodName 
            << " decLowerLimitDeg: " << decLowerLimitDeg
            << " decUpperLimitDeg: " << decUpperLimitDeg
            << endl;);

   // Restrict RA Hour range, if not too near the North Pole.
   stringstream raStrm;
   double maxDecCutoffForRaLimitsInDeg(85);
   if (beamCenterDecDeg < maxDecCutoffForRaLimitsInDeg)
   {
      double beamRadiusRads(SseAstro::degreesToRadians(maxPrimaryBeamsizeDeg/2));

      double raUpperLimitRads;
      double dummyDecRads;
      OffPositions::moveEast(primaryFovCenterRaDec2000_.ra.getRadian(),
                             primaryFovCenterRaDec2000_.dec.getRadian(),
                             beamRadiusRads,
                             &raUpperLimitRads, &dummyDecRads);

      double raLowerLimitRads;
      OffPositions::moveWest(primaryFovCenterRaDec2000_.ra.getRadian(),
                             primaryFovCenterRaDec2000_.dec.getRadian(),
                             beamRadiusRads,
                             &raLowerLimitRads, &dummyDecRads);

      double raUpperLimitHours(SseAstro::radiansToHours(raUpperLimitRads));
      double raLowerLimitHours(SseAstro::radiansToHours(raLowerLimitRads));

      // Handle wrap around hour zero
      string raConjunction("and");
      if (raLowerLimitHours > raUpperLimitHours)
      {
         raConjunction = "or";
      }
      raStrm << " (ra2000Hours >= " << raLowerLimitHours
             << " " << raConjunction 
             << " ra2000Hours <= " << raUpperLimitHours << ")";
      
      VERBOSE2(getVerboseLevel(), methodName 
               << " raUpperLimitHours: " << raUpperLimitHours
               << " raLowerLimitHours: " << raLowerLimitHours
               << " restriction: " << raStrm.str()
               << endl;);
   }
   else
   {
      // No RA restriction
      raStrm << " true ";
   }


   stringstream queryRestrictStrm;
   queryRestrictStrm << " and " << raStrm.str();

   if (usePreselectedPrimaryFovCenter_)
   {
      // Get all targets in primary FOV regardless of priority
      // No catalog restriction.
   }
   else
   {
      // Assume high priority targets have
      // already been loaded, so only get
      // the low priority targets needed to fill 
      // in the field of view.

      queryRestrictStrm << prepareQueryRestrictionForCatalogs(lowPriorityCatalogs_);
   }
   
   getTargets(targetMap, 
              decLowerLimitDeg, decUpperLimitDeg,
              queryRestrictStrm.str());
}

void OrderedTargets::resetCulledTargetMap()
{
   if (usePreselectedPrimaryFovCenter_)
   {
      culledTargetMap_ = onDemandTargetMap_;
   }
   else
   {
      culledTargetMap_ = highPriorityTargetMap_;
   }

   selectionLogStrm_ << "OrderedTargets::resetCulledTargetMap: "
                     << "number of targets loaded from db: " 
                     << culledTargetMap_.size() << endl;
}

void OrderedTargets::setObsDate(time_t obsDate)
{
   const string methodName("OrderedTargets::setObsDate: ");

   obsDate_ = obsDate;

   lmstRads_ = siteView_->lmstRads(obsDate_);

   VERBOSE2(getVerboseLevel(), methodName 
	    << "obsDate: " << SseUtil::isoDateTime(obsDate_)
	    <<", lmst hours: " 
	    << SseAstro::radiansToHours(lmstRads_)
	    << endl;);    

   selectionLogStrm_ << methodName << " LMST hours: " 
                     <<  SseAstro::radiansToHours(lmstRads_) << endl;

}

void OrderedTargets::setObsDateOnTargets(TargetMap & targetMap)
{
   const string methodName("OrderedTargets::setObsDateOnTargets: ");

   for (TargetMap::iterator it = targetMap.begin();
	it != targetMap.end(); ++it)
   {
      Target *target = (*it).second;
      target->setTime(lmstRads_, obsDate_);
   }

}

void OrderedTargets::eraseTarget(TargetMap::iterator it, 
				 TargetMap & targetMap)
{
   Assert (it != targetMap.end());

   targetMap.erase(it);
}


void OrderedTargets::prepareDesiredObservingFreqRanges(
   const Range& freqRangeLimitsMhz,
   vector<FrequencyBand> & permRfiMaskFreqBands)
{
   desiredObservingFreqRangesMhz_.addInOrder(
      freqRangeLimitsMhz.low_, freqRangeLimitsMhz.high_);

   desiredObservingFreqRangesMhz_ = desiredObservingFreqRangesMhz_ -
      permRfiMaskFreqBands;
}


void OrderedTargets::getTargets(
   TargetMap & targetMap, 
   double decLowerLimitDeg,
   double decUpperLimitDeg,
   const string &queryRestriction)
{
   string methodName("OrderedTargets::getTargets: ");

   VERBOSE2(getVerboseLevel(), methodName << endl;);    

   stringstream targetCatQuery;

   targetCatQuery << "select targetId, primaryTargetId, "
		  << "ra2000Hours, dec2000Deg, "
                  << "pmRaMasYr, pmDecMasYr, parallaxMas, catalog "
		  << "from TargetCat WHERE ";
   
   // only consider targets tagged for automatic selection
   targetCatQuery << " autoSchedule = 'Yes'";

   targetCatQuery << " " << queryRestriction;

   // set declination limits
   targetCatQuery << " and dec2000Deg >= " << decLowerLimitDeg
      		  << " and dec2000Deg <= " << decUpperLimitDeg;

   VERBOSE2(getVerboseLevel(), methodName 
	    << " query: " << targetCatQuery.str() << endl;);    

   enum resultCols { targetIdCol, primaryTargetIdCol,
		     ra2000HoursCol, dec2000DegCol,
		     pmRaCol, pmDecCol, parallaxCol, catalogCol, numCols };

   MysqlQuery query(db_);
   query.execute(targetCatQuery.str(), numCols, __FILE__, __LINE__);
  
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      TargetId targetId(query.getInt(row, targetIdCol, 
				     __FILE__, __LINE__));

      TargetId primaryTargetId(query.getInt(row, primaryTargetIdCol, 
				     __FILE__, __LINE__));

      double ra2000Rads(SseAstro::hoursToRadians(
	 query.getDouble(row, ra2000HoursCol, __FILE__, __LINE__)));
      
      double dec2000Rads = SseAstro::degreesToRadians(
	 query.getDouble(row, dec2000DegCol, __FILE__, __LINE__));
      
      double pmRaMasYr(query.getDouble(row, pmRaCol, __FILE__, __LINE__));
   
      double pmDecMasYr(query.getDouble(row, pmDecCol, __FILE__, __LINE__));
      
      double parallaxMas(query.getDouble(row, parallaxCol,
                                         __FILE__, __LINE__));

      string catalog(query.getString(row, catalogCol,
                                     __FILE__, __LINE__));

      Target *target = new Target(
	 RaDec(Radian(ra2000Rads), Radian(dec2000Rads)),
	 pmRaMasYr, pmDecMasYr,
	 parallaxMas,
	 targetId, primaryTargetId,
	 catalog,
	 desiredObservingFreqRangesMhz_, 
         maxDistLightYears_,
	 siteView_,
	 obsLengthSec_,
	 minAcceptableRemainingBandMhz_,
	 minRemainingTimeOnTargetRads_
	 );

      Target::CatalogPriority priority;
      int order;
      lookupCatalogPriority(catalog, priority, order);
      target->setCatalogPriority(priority, order);

#if 0
// DEBUG
      cout << "id: " <<  targetId << " cat: " << catalog << " priority: " << priority 
           << " order: " << order <<endl;
#endif


      targetMap[target->getTargetId()] = target;
   }

   VERBOSE2(getVerboseLevel(), methodName << "number of targets: " 
	    << targetMap.size()  << endl;);    

}


/*
  Get the observing history based on dx tuning information in
  the ActivityUnits table. Only valid observations are used.
*/
void OrderedTargets::retrieveObsHistFromDbForTargets(TargetMap & targetMap)
{
   string methodName("retrieveObsHistFromDbForTargets");
   VERBOSE2(getVerboseLevel(), "OrderedTargets::" << methodName << endl;);    

   stringstream sqlstmt;
   sqlstmt << " select targetId, dxLowFreqMhz, dxHighFreqMhz"
           << " from ActivityUnits"
           << " where "
           << " validObservation = 'Yes'"
           << " and dxLowFreqMhz >= " << freqRangeLimitsMhz_.low_
           << " and dxLowFreqMhz <= " << freqRangeLimitsMhz_.high_;

   // restrict to targetIds in the map
   sqlstmt << " and targetId in (";
   for (TargetMap::iterator it = targetMap.begin();
        it != targetMap.end(); ++it)
   {
      sqlstmt << it->first << ", ";
   }
   sqlstmt << "-1)";

   sqlstmt << " order by dxLowFreqMhz, dxHighFreqMhz";

   enum colIndices {targetIdCol, lowFreqMhzCol, highFreqMhzCol, numCols};

   VERBOSE3(getVerboseLevel(), "OrderedTargets::" << methodName
            << " query: " << sqlstmt.str() << endl;);    
   
   MysqlQuery query(db_);
   query.execute(sqlstmt.str(), numCols, __FILE__, __LINE__);

   VERBOSE2(getVerboseLevel(), "OrderedTargets::" << methodName
            << " query returned " << mysql_num_rows(query.getResultSet())
            << " rows" << endl;);    

   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      TargetId targetId(query.getInt(
         row, targetIdCol, __FILE__, __LINE__));
                        
      double lowFreqMhz(query.getDouble(
         row, lowFreqMhzCol, __FILE__, __LINE__));
      
      double highFreqMhz(query.getDouble(
         row, highFreqMhzCol, __FILE__, __LINE__));

      TargetMap::iterator mapIt = targetMap.find(targetId);
      Assert(mapIt != targetMap.end());

      Target *target = mapIt->second;
      target->addObserved(lowFreqMhz, highFreqMhz);
   }

   VERBOSE2(getVerboseLevel(), "OrderedTargets::" << methodName
	    << " complete " << endl;);    

}

// Get Observed frequencies for the zxs
//
void OrderedTargets::retrieveObsHistFromDbForZxs(ObsRange &observedFreqs,
		TargetId target)
{
   string methodName("retrieveObsHistFromDbForZxs");
   VERBOSE2(getVerboseLevel(), "OrderedTargets::" << methodName << endl;);    

   stringstream sqlstmt;
   sqlstmt << " select distinct dxLowFreqMhz, dxHighFreqMhz"
           << " from ActivityUnits"
           << " where "
           << " validObservation = 'Yes'"
           << " and targetId = " << target 
           << " and (dxNumber = 1900 or dxNumber = 2900 or dxNumber = 3900)"
   	<< " order by dxLowFreqMhz, dxHighFreqMhz";

   enum colIndices {lowFreqMhzCol, highFreqMhzCol, numCols};

   VERBOSE3(getVerboseLevel(), "OrderedTargets::" << methodName
            << " query: " << sqlstmt.str() << endl;);    
   
   MysqlQuery query(db_);
   query.execute(sqlstmt.str(), numCols, __FILE__, __LINE__);

   VERBOSE2(getVerboseLevel(), "OrderedTargets::" << methodName
            << " query returned " << mysql_num_rows(query.getResultSet())
            << " rows" << endl;);    

   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      double lowFreqMhz(query.getDouble(
         row, lowFreqMhzCol, __FILE__, __LINE__));
      
      double highFreqMhz(query.getDouble(
         row, highFreqMhzCol, __FILE__, __LINE__));

      cout << " Low " << lowFreqMhz << " High " << highFreqMhz << endl;
      observedFreqs.addInOrder(lowFreqMhz, highFreqMhz);
   }

   VERBOSE2(getVerboseLevel(), "OrderedTargets::" << methodName
	    << " complete " << endl;);    

}


void OrderedTargets::updateObservedFreqsForTargetsFromDbObsHistory(ActivityId_t actId)
{
   TargetMap allTargetsMap;

   allTargetsMap.insert(highPriorityTargetMap_.begin(), highPriorityTargetMap_.end());
   allTargetsMap.insert(onDemandTargetMap_.begin(), onDemandTargetMap_.end());

   getObservedFreqsForActivity(allTargetsMap, actId);
}



// Retrieves the observed frequencies
// from the database ActivityUnits table for an Activity.
// The frequencies for each target in the targetMap are added to that target's
// observed frequency range.

void OrderedTargets::getObservedFreqsForActivity(
   TargetMap &targetMap,
   ActivityId_t activityId)
{
   string methodName("::getObservedFreqsForActivity");

   stringstream sqlstmt;
   sqlstmt << " select targetId, dxLowFreqMhz, dxHighFreqMhz"
	   << " from ActivityUnits "
	   << " where "
	   << " activityId = " << activityId 
	   << " order by dxLowFreqMhz, dxHighFreqMhz ";

   //cout << methodName << ":" << sqlstmt.str() << endl;

   enum colIndices { targetIdCol, lowFreqMhzCol, highFreqMhzCol, numCols };

   Assert(db_);

   MysqlQuery query(db_);
   query.execute(sqlstmt.str(), numCols, __FILE__, __LINE__);
   
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      TargetId targetId(query.getInt(row, targetIdCol, 
				     __FILE__, __LINE__));
      
      double lowFreqMhz(query.getDouble(
	 row, lowFreqMhzCol, __FILE__, __LINE__));
      
      double highFreqMhz(query.getDouble(
	 row, highFreqMhzCol, __FILE__, __LINE__));

      //cout << methodName << " found target " << targetId << endl;
      //cout << methodName << " adding in " << lowFreqMhz 
      //   << " to " << highFreqMhz << endl;

      TargetMap::iterator targetIter = targetMap.find(targetId);
      if (targetIter != targetMap.end())
      {
	 Target *target = (*targetIter).second;
	 target->addObservedOutOfOrder(lowFreqMhz, highFreqMhz);

	 //cout << "target unobs " << target->unobserved() << endl;
      } 
      else
      {
	 // Target should always be found 
	 stringstream strm;
	 strm << __FILE__ << methodName << " target " 
	      << targetId << " not found in targetMap "
	      << "\n";

	 SseArchive::ErrorLog() << strm.str();
	 SseArchive::SystemLog() << strm.str();

         // TBD enable this exception instead
	 //throw SseException(strm.str(), __FILE__,  __LINE__, 
	 //		    SSE_MSG_DBERR, SEVERITY_ERROR);

      }
   }
}


/* 
 Find targets whose remaining to-be-observed frequency
 coverage is less than the minimum.  Tag them as already
 completely observed, and reset their observations back
 to the "unobserved" state so that they can be observed again
 but at a lower priority.

 Selection criteria:

 Make sure at least one subrange is greater than the smallest dx
 bandwidth (times a fudge factor to account for scheduler tuning
 quirks).
 
 Also make sure that the sum of the subranges is greater than the
 minimum acceptable amount of remaining bandwidth to be observed (but
 only count the subranges greater than the above minimum dx
 bandwidth).  This allows targets to be observed even if small perm
 RFI mask regions break up the to-be-observed chunks into pieces
 smaller than the overall minimum.
*/
void OrderedTargets::resetObservationsForCompletelyObservedTargets(
   TargetMap & targetMap)
{
   string methodName("OrderedTargets::resetObservationsForCompletelyObservedTargets: ");

   VERBOSE2(getVerboseLevel(), methodName << endl;);    

   double minRangeElementBwMhz = 2 * bandwidthOfSmallestDxMhz_;
   /*
     Handle the case where only one or two dxs are being used
     and the frequency range to be observed is only one or 
     two dx's bandwidth wide.
   */
   minRangeElementBwMhz = min(minRangeElementBwMhz,
                              minAcceptableRemainingBandMhz_);

   unsigned int alreadyObservedCount(0);
   for (TargetMap::iterator it = targetMap.begin();
	it != targetMap.end(); ++it)
   {
      Target *target = (*it).second;

      if (target->hasRangeGt(minRangeElementBwMhz) &&
	  (target->totalRangeGt(minRangeElementBwMhz) >
	   minAcceptableRemainingBandMhz_))
      {
	 continue;
      } 
      else
      {
	 // Target has already been fully observed at least once.
	 // Reset it so that it can be observed again.
	 target->resetObservations();
	 target->setAlreadyCompletelyObserved(true);

	 // If the observed range is empty, it means that
	 // the frequency constraints imposed on target
	 // selections are such that it's impossible to observe
	 // any targets.

	 if (target->unobserved().isEmpty())
	 {
	    stringstream errorStrm;
 
	    errorStrm << "No unobserved frequencies available for targets "
		      << "(check scheduler begin/end freqs, "
		      << "minimum acceptable bandwidth, and perm RFI mask)\n";

	    throw SseException(errorStrm.str(), __FILE__, __LINE__,
	       SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
	 }

	 alreadyObservedCount++;

      }
   }

   VERBOSE2(getVerboseLevel(), methodName << alreadyObservedCount
	    << " targets were already fully observed" << endl;);    

   if (targetMap.size() > 0 && 
       alreadyObservedCount == targetMap.size())
   {
      SseArchive::SystemLog() 
         << "Scheduler note: all " << alreadyObservedCount
         << " targets have been fully observed"
         << " for the given frequency range and minimum dx"
         << " occupancy settings.  Scheduler will choose targets"
         << " for reobservation.\n";
   }
}

// Remove any targets from the Target List that are not currently visible
//
void OrderedTargets::cullNotVisible(TargetMap &targetMap)
{
   string methodName("OrderedTargets::cullNotVisible: ");
   VERBOSE2(getVerboseLevel(), methodName << endl;);    

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      Target * target = (*i).second;

      // debug
      //cout << "Target: " << (*i).first << " " << target->getRaDec() << " ";

      double keepTarget(false);
      if (target->isVisible())
      {
	 // make sure there's enough time before target sets
	 // for sufficient number of followup observations 

	 if (target->getTimeUntilSetRads() > 
	     getMinRemainingTimeOnTargetRads())
	 {
	    keepTarget = true;

	    // debug
	    //cout << " visible, time left is " 
	    //	 << target->getTimeUntilSetRads() << " rads";
	 }
      }

      if (keepTarget)
      {
	 i++;
      }
      else
      {
	 eraseTarget(i++, targetMap);
      }
      
      // debug
      //cout << endl;

   }

   VERBOSE2(getVerboseLevel(), methodName << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
} 

void OrderedTargets::createPrimaryTargetIdHistogram(const TargetMap & targetMap,
                                                    HistogramMap & histMap)
{
   for (TargetMap::const_iterator it = targetMap.begin();
	it != targetMap.end(); ++it)
   {
      Target * target = (*it).second;
      TargetId primaryTargetId = target->getPrimaryTargetId();

      // initialize counter for this primary target id
      if (histMap.find(primaryTargetId) == histMap.end())
      {
	 histMap[primaryTargetId] = 0;
      }

      histMap[primaryTargetId] += 1;
   }

#if 1
   // debug:
   // print histogram:

   stringstream strm;
   strm << "primary target id histogram:" << endl;

   for (HistogramMap::const_iterator histIt = histMap.begin();
	histIt != histMap.end(); ++histIt)
   {
      strm << histIt->first << "  " 
	   << histIt->second << endl;
   }
   VERBOSE2(getVerboseLevel(), strm.str());
   
#endif

}


/* Find the primary target regions that have only a small portion
 * of their area visible, and cull the targets in those regions.
 * This will handle the case where the region is just visible enough to
 * allow the selection of the first target, but not visible enough
 * to allow additional targets to be chosen.
 */
void OrderedTargets::cullByPartiallyBlockedPrimaryTargetRegions(TargetMap & targetMap)
{
   string methodName("OrderedTargets::cullByPartiallyBlockedPrimaryTargetRegions: ");
   VERBOSE2(getVerboseLevel(), methodName << endl;);    

   HistogramMap histMap;
   createPrimaryTargetIdHistogram(targetMap, histMap);

   /*
     Eliminate all the targets from the primary regions that
     are below the minimum cutoff count.
     Only count targets that have a valid (non-default)
     primaryTargetId assigned to them, i.e., primaryTargetId > 0.
   */

   for (TargetMap::iterator it = targetMap.begin();
	it != targetMap.end(); )
   {
      Target * target = (*it).second;
      TargetId primaryTargetId = target->getPrimaryTargetId();

      if ((primaryTargetId > 0) && 
          (histMap[primaryTargetId] < primaryTargetIdCountCutoff_))
      {
         eraseTarget(it++, targetMap);
      } 
      else
      {
         it++;
      } 
   }

   VERBOSE2(getVerboseLevel(), methodName << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
}


void OrderedTargets::cullBySunAvoidAngle(TargetMap & targetMap)
{
   string methodName("OrderedTargets::cullBySunAvoidAngle: ");

   double sunRaRads;
   double sunDecRads;
   SseAstro::sunPosition(obsDate_, &sunRaRads, &sunDecRads);

   VERBOSE2(getVerboseLevel(), methodName
	    << " sun Ra (Hrs), Dec (deg) = "
            << SseAstro::radiansToHours(sunRaRads)
	    << " " << SseAstro::radiansToDegrees(sunDecRads)
            << ", avoid angle (deg) = " << sunAvoidAngleDeg_ << endl;);    

   // TBD get avoidance angle adjusted by observing frequency
   double sunAvoidAngleRads(SseAstro::degreesToRadians(sunAvoidAngleDeg_));

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      RaDec targetRaDec = (*i).second->getRaDec();

      double targetSunSepRads = SseAstro::angSepRads(
         targetRaDec.ra, targetRaDec.dec,
         sunRaRads, sunDecRads);

      if (targetSunSepRads < sunAvoidAngleRads)
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 
   }

   VERBOSE2(getVerboseLevel(), methodName << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
} 

void OrderedTargets::cullByMoonAvoidAngle(TargetMap &targetMap)
{
   string methodName("OrderedTargets::cullByMoonAvoidAngle: ");

   /*
     The moon position is geocentric, so the value might be
     off by as much as ~1 degree from the topocentric
     position.  But the avoidance angle is assumed to be much
     larger than this, so the low precision should not be 
     a problem.
    */

   double moonRaRads;
   double moonDecRads;
   SseAstro::moonPosition(obsDate_, &moonRaRads, &moonDecRads);

   VERBOSE2(getVerboseLevel(), methodName
	    << " moon Ra (Hrs), Dec (deg) = "
            << SseAstro::radiansToHours(moonRaRads)
	    << " " << SseAstro::radiansToDegrees(moonDecRads)
            << ", avoid angle (deg) = " << moonAvoidAngleDeg_ << endl;);    

   // TBD get avoidance angle adjusted by observing frequency
   double moonAvoidAngleRads(SseAstro::degreesToRadians(moonAvoidAngleDeg_));

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      RaDec targetRaDec = (*i).second->getRaDec();
      
      double targetMoonSepRads = SseAstro::angSepRads(
         targetRaDec.ra, targetRaDec.dec,
         moonRaRads, moonDecRads);

      if (targetMoonSepRads < moonAvoidAngleRads)
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 
   }

   VERBOSE2(getVerboseLevel(), methodName << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
} 


void OrderedTargets::cullByGeosatAvoidAngle(TargetMap & targetMap)
{
   string methodName("OrderedTargets::cullByGeosatAvoidAngle: ");

   double geosatDecDeg(SseAstro::geosatDecDeg(
                          SseAstro::radiansToDegrees(siteView_->getLatRads())));
   double geosatDecRads(SseAstro::degreesToRadians(geosatDecDeg));
   double geosatAvoidAngleRads(SseAstro::degreesToRadians(geosatAvoidAngleDeg_));
   double upperDecLimitRads(geosatDecRads + geosatAvoidAngleRads);
   double lowerDecLimitRads(geosatDecRads - geosatAvoidAngleRads);

   VERBOSE2(getVerboseLevel(), methodName
	    << " geosatDecDeg = " << geosatDecDeg
            << ", avoid angle (deg) = " << geosatAvoidAngleDeg_ << endl;);    

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      RaDec targetRaDec = (*i).second->getRaDec();

      if ((targetRaDec.dec < upperDecLimitRads) 
          && (targetRaDec.dec > lowerDecLimitRads))
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 
   }

   VERBOSE2(getVerboseLevel(), methodName << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
} 


void OrderedTargets::cullByZenithAvoidAngle(TargetMap & targetMap)
{
   string methodName("OrderedTargets::cullByZenithAvoidAngle: ");

   double zenithRaRads(lmstRads_);
   double zenithDecRads(siteView_->getLatRads());
   double zenithAvoidAngleRads(SseAstro::degreesToRadians(zenithAvoidAngleDeg_));

   VERBOSE2(getVerboseLevel(), methodName
	    << " zenithRaHours = " << SseAstro::radiansToHours(zenithRaRads)
	    << ", zenithDecDeg = " << SseAstro::radiansToDegrees(zenithDecRads)
            << ", avoid angle (deg) = " << zenithAvoidAngleDeg_ << endl;);    

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      RaDec targetRaDec = (*i).second->getRaDec();
      
      double targetZenithSepRads = SseAstro::angSepRads(
         targetRaDec.ra, targetRaDec.dec,
         zenithRaRads, zenithDecRads);

      if (targetZenithSepRads < zenithAvoidAngleRads)
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 
   }

   VERBOSE2(getVerboseLevel(), methodName << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
} 



// Remove all targets from targetMap that don't share the primaryTargetId
void OrderedTargets::cullByPrimaryTargetId(TargetMap &targetMap,
                                           TargetId primaryTargetIdToKeep)
{
   string methodName("OrderedTargets::cullByPrimaryTargetId");

   VERBOSE2(getVerboseLevel(), methodName << " primaryTargetId: " << 
	    primaryTargetIdToKeep << endl;);

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      TargetId primaryTargetId = (*i).second->getPrimaryTargetId();
      
      if (primaryTargetId != primaryTargetIdToKeep)
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 

   }

   VERBOSE2(getVerboseLevel(), methodName << ": " << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;

} 


// Remove all targets from targetMap that fall outside of the
// primary field of view.
void OrderedTargets::cullOutsidePrimaryFov(
   TargetMap & targetMap,
   const RaDec & centerRaDec,
   double primaryBeamsizeRads)
{
   string methodName("OrderedTargets::cullOutsidePrimaryFov: ");

   VERBOSE2(getVerboseLevel(), methodName
	    << ": center RaDec = " << centerRaDec << " primaryBeamsizeRads = "
	    << primaryBeamsizeRads << endl;);    

   double primaryBeamRadiusRads = primaryBeamsizeRads / 2;

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      RaDec targetRaDec = (*i).second->getRaDec();
      
      double angSepRads = SseAstro::angSepRads(
        targetRaDec.ra, targetRaDec.dec,
        centerRaDec.ra, centerRaDec.dec);

      if (angSepRads > primaryBeamRadiusRads)
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 

   }

   VERBOSE2(getVerboseLevel(), methodName << ": " << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName <<"center RaDec = " << centerRaDec 
                     << ", targets left: "
                     << targetMap.size() << endl;

} 


// Remove all targets from targetMap that are within the minimum
// target separation of the chosenTargetId (including the chosenTargetId)

void OrderedTargets::cullByMinTargetSeparation(
   TargetMap & targetMap, TargetId chosenTargetId, double minSepRads)
{
   string methodName("OrderedTargets::cullByMinTargetSeparation: ");

   // make sure target is in the map
   Assert(targetMap.find(chosenTargetId) != targetMap.end());
   RaDec chosenTargetRaDec = targetMap[chosenTargetId]->getRaDec();

   VERBOSE2(getVerboseLevel(), methodName
	    << "chosenTargetId = " << chosenTargetId
	    << ": chosenTarget RaDec = " << chosenTargetRaDec 
	    << " minSepRads = " << minSepRads
	    << endl;);    

   cout <<  methodName << "chosenTargetId = " << chosenTargetId
	    << ": chosenTarget RaDec = " << chosenTargetRaDec 
	    << " minSepRads = " << minSepRads
	    << endl;  

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      RaDec targetRaDec = (*i).second->getRaDec();

      double angSepRads = SseAstro::angSepRads(
         targetRaDec.ra, targetRaDec.dec,
         chosenTargetRaDec.ra, chosenTargetRaDec.dec);
      
      if (angSepRads < minSepRads)
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 

   }

   VERBOSE2(getVerboseLevel(), methodName << ": " << targetMap.size() 
	    << " targets remain after culling" << endl;);    
    cout <<  methodName << ": " << targetMap.size() 
	    << " targets remain after culling" << endl;    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
} 




// If current target has not been culled and has not been 
// fully observed, it is returned,
// else the bestObsRise() target is found.

OrderedTargets::TargetMap::const_iterator
OrderedTargets::bestObsRise(TargetMap & targetMap, TargetId currentTargetId)
{
   TargetMap::const_iterator currentTargetIter =
      targetMap.find(currentTargetId);

   if (currentTargetIter != targetMap.end() &&
       ! (*currentTargetIter).second->isAlreadyCompletelyObserved())
   {
      return currentTargetIter;
   }
   else
   {
      // pick a new target
      return bestObsRise(targetMap);
   }

}

// Find the target with the highest overall merit

OrderedTargets::TargetMap::const_iterator
OrderedTargets::bestObsRise(TargetMap &targetMap)
{
   TargetMap::const_iterator best =
      targetMap.begin();
   TargetMap::const_iterator bestNearRise =
      targetMap.end();
   if (best == targetMap.end()){

      SseArchive::SystemLog() << selectionLogStrm_.str();
      SseArchive::ErrorLog() << selectionLogStrm_.str();

      throw SseException(
         "No targets are available within the observing constraints.\n",
         __FILE__,  __LINE__, SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
   }

   for (TargetMap::const_iterator index = best;
	index != targetMap.end(); index++)
   {
      if (targetMerit_->overallMerit((*index).second) >
	  targetMerit_->overallMerit((*best).second))
      {
	 best = index;
      }

      double timeSinceRiseRads = (*index).second->getTimeSinceRiseRads();
      if (Hour(Radian(timeSinceRiseRads)).getMinutes() 
	  < autoRiseTimeCutoffMinutes_)
      {
	 if ((bestNearRise == targetMap.end()) ||
	     (targetMerit_->overallMerit((*index).second) >
	      targetMerit_->overallMerit((*bestNearRise).second)))
	 {
	    bestNearRise = index;
	 }
      }
   }

   if (!autorise_)
   {
      return best;
   }
   else if (bestNearRise == targetMap.end() || 
	    (targetMerit_->overallMerit((*bestNearRise).second) <= 0.0))
   {
      SseArchive::SystemLog() << "target near rising could not be found\n";
      return best;
   }
   else {
      SseArchive::SystemLog() << "found target near rising\n";
      return bestNearRise;
   }
}

void OrderedTargets::prepareTargetMap(time_t obsDate)
{
   selectionLogStrm_.str("");  // reset log
   selectionLogStrm_ << "Scheduler target culling:\n";

   setObsDate(obsDate);

   resetCulledTargetMap();

   setObsDateOnTargets(culledTargetMap_);

   cullNotVisible(culledTargetMap_);

   cullBySunAvoidAngle(culledTargetMap_);

   cullByMoonAvoidAngle(culledTargetMap_);

   cullByGeosatAvoidAngle(culledTargetMap_);

   cullByZenithAvoidAngle(culledTargetMap_);

   /*
     Do this last, so that only complete primary target 
     regions remain.
   */
   cullByPartiallyBlockedPrimaryTargetRegions(culledTargetMap_);
}




void OrderedTargets::useThisPrimaryFovCenter(
   double ra2000Rads, double dec2000Rads)
{
   const string methodName("useThisPrimaryFovCenter: ");
   VERBOSE2(getVerboseLevel(), methodName << endl;);

   usePreselectedPrimaryFovCenter_ = true;

   RaDec requestedPosition;
   requestedPosition.ra = Radian(ra2000Rads);
   requestedPosition.dec = Radian(dec2000Rads);

   setPrimaryBeamCenter(requestedPosition);
}

void OrderedTargets::setPrimaryBeamCenter(const RaDec &requestedPosition)
{
   const string methodName("OrderedTargets::setPrimaryBeamCenter :");

   /* See if it's changed since last time, if it has
    then update, otherwise ignore */

   // TBD: is this a reasonable tolerance?
   // How much does the beam position vary when
   // pointing at the "same" position?

   double tolRads(10e-6);

   double angSepRads = SseAstro::angSepRads(
      requestedPosition.ra, requestedPosition.dec,
      primaryFovCenterRaDec2000_.ra, primaryFovCenterRaDec2000_.dec);

   if (angSepRads > tolRads)
   {
      VERBOSE2(getVerboseLevel(), methodName 
               << " New primary FOV received: " 
               << requestedPosition << endl;);

      primaryFovCenterRaDec2000_ = requestedPosition;
      onDemandTargetMapUpToDate_ = false;
   }
   
}

// Gathers all visible targets and previous observations.
// Selects best first target, plus additional targets as needed.

void OrderedTargets::chooseTargets(
   int nTargetsToChoose,
   time_t obsDate,
   double minTargetSeparationInBeams,  
   bool areActsRunning,
   TargetId & firstTargetId,  // returned target id
   ObsRange & chosenObsRange,   // returned freqs to be observed
   TargetId & primaryTargetId,  // returned
   TargetIdSet & additionalTargetIds  // returned target ids
   )
{
   if (usePreselectedPrimaryFovCenter_)
   {
      prepareOnDemandTargets();
   }
   else
   {
      prepareHighPriorityTargets();
   }

   prepareTargetMap(obsDate);

   resetObservationsForCompletelyObservedTargets(
      culledTargetMap_);

   /*
     Use a temporary copy of the target list
     when choosing the first target, so that
     we don't do cullings that would exclude 
     choices for additional target selections.
    */
   TargetMap firstTargetMap = culledTargetMap_;
   if (usePreselectedPrimaryFovCenter_)
   { 
      /*
        Use the lowest unobserved freq on each target to
        determine the primary beam size that would be needed to 
        see it.  Cull it if it's too far from the center.
      */

      if (! isPositionVisible(primaryFovCenterRaDec2000_))
      {
         stringstream strm;

         strm << "primary beam FOV center is not visible: " 
              << primaryFovCenterRaDec2000_ << endl;

	 throw SseException(strm.str(), __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }

      cullTooFarFromPrimaryFovCenter(firstTargetMap,
                                     primaryFovCenterRaDec2000_);
   }

   // Try to reuse current target, if possible
   TargetId currentFirstTargetId = firstChosenTargetId_;

   TargetId bestTargetId;
   getBestTarget(firstTargetMap, currentFirstTargetId, areActsRunning,
		 bestTargetId, chosenObsRange);

   // remember for next time
   firstChosenTargetId_ = bestTargetId;

   // values to return:
   firstTargetId = bestTargetId;
   primaryTargetId = determinePrimaryTargetId(firstTargetMap, firstTargetId);

   SseArchive::SystemLog() << "Primary beam target: " << primaryTargetId
			   << endl;

   if (nTargetsToChoose > 1)
   {
      chooseAdditionalTargets(nTargetsToChoose-1, 
                              minTargetSeparationInBeams, 
                              additionalTargetIds);
   }
}

void OrderedTargets::cullTooFarFromPrimaryFovCenter(TargetMap & targetMap,
                                                    RaDec & primaryFovCenter)
{
   /*
     Look at the lowest unobserved freq on each target.  Add in the
     maxDxTuningSpreadMhz to determine the highest possible frequency
     that could be used in the next observation.  Use that
     value to define the primary beam size that would be needed
     to see that target.  Cull it if it's too far from the center.
   */
   string methodName("OrderedTargets::cullTooFarFromPrimaryFovCenter: ");

   for (TargetMap::iterator i = targetMap.begin();
	i != targetMap.end(); )
   {
      Target *target = (*i).second;
      RaDec targetRaDec = target->getRaDec();
      double maxSkyFreqMhz = target->unobserved().minValue()
         + maxDxTuningSpreadMhz_;

      double primaryBeamRadiusRads = AtaInformation::ataBeamsizeRadians(
         maxSkyFreqMhz, primaryBeamsizeAtOneGhzArcSec_) / 2.0;

      double angSepRads = SseAstro::angSepRads(
                targetRaDec.ra, targetRaDec.dec,
                primaryFovCenter.ra, primaryFovCenter.dec); 
      
      if (angSepRads > primaryBeamRadiusRads)
      {
	 eraseTarget(i++, targetMap);
      } 
      else
      {
	 i++;
      } 

   }

   VERBOSE2(getVerboseLevel(), methodName << ": " << targetMap.size() 
	    << " targets remain after culling" << endl;);    

   selectionLogStrm_ << methodName << "targets left: "
                     << targetMap.size() << endl;
}

// Choose more targets that are all at least beamsizeRads apart from
// the first target (chosen earlier) and each other.  Also all
// targets must fall in the telescope primary field of view (FOV).  
// Ideally targets will be chosen that have not yet observed the target 
// frequencies selected for the first target (not yet implemented),
// but if necessary targets can be reobserved.
// Secondary targets currently being used have priority.

// TBD: prefer targets with unobserved freq ranges matching the primary target

void OrderedTargets::chooseAdditionalTargets(
   int nTargetsToChoose,
   double minTargetSeparationBeamsizes,  
   TargetIdSet & chosenTargetIds  // returned target ids
   )
{
   string methodName("OrderedTargets::chooseAdditionalTargets: ");

   VERBOSE2(getVerboseLevel(), methodName << endl;);

   // make sure previously chosen first target is in the map
   Assert(culledTargetMap_.find(firstChosenTargetId_) != culledTargetMap_.end());
   Target * firstChosenTarget = culledTargetMap_[firstChosenTargetId_];

   double maxSkyFreqMhz = firstChosenTarget->unobserved().minValue()
      + maxDxTuningSpreadMhz_;

   /* Restrict targets to primary FOV.  This is done in one of three ways:
    * 1. If the target has a valid primaryTargetId assigned to it, 
    *    then eliminate all targets that don't share that primary pointing.
    *    It's assumed that all targets assigned to the same primary target Id
    *    all fall in the same primary FOV.
    *
    * 2. Use the first target Id as the center of the FOV, and eliminate
    *    all targets outside that FOV.
    *
    * 3. Use the preassigned primary FOV, and eliminate targets outside.
    */

   TargetId primaryTargetId = firstChosenTarget->getPrimaryTargetId();
   if (primaryTargetId > 0)
   {
      cullByPrimaryTargetId(culledTargetMap_, primaryTargetId);
      if (culledTargetMap_.size() < 1)
      {
	 throw SseException(
	    "no targets remain after culling by primary target Id\n",
	    __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }
   }
   else
   {
      RaDec centerRaDec;
      if (usePreselectedPrimaryFovCenter_)
      {
         centerRaDec = primaryFovCenterRaDec2000_;

         // All on-demand targets for this primary FOV are already
         // loaded.
      }         
      else
      {
         // Position of first target becomes center of the primary FOV.
         centerRaDec = firstChosenTarget->getRaDec();

         setPrimaryBeamCenter(centerRaDec);

         // Get additional targets for this primary beam FOV
         prepareOnDemandTargets();
         
         setObsDateOnTargets(onDemandTargetMap_);
         
         resetObservationsForCompletelyObservedTargets(
            onDemandTargetMap_);

         culledTargetMap_.insert(onDemandTargetMap_.begin(),
                                 onDemandTargetMap_.end());
      }

      double primaryBeamsizeRads = AtaInformation::ataBeamsizeRadians(
         maxSkyFreqMhz, primaryBeamsizeAtOneGhzArcSec_);

      cullOutsidePrimaryFov(culledTargetMap_, centerRaDec, primaryBeamsizeRads);
      if (culledTargetMap_.size() < 1)
      {
	 throw SseException(
	    "no targets remain after culling by primary FOV\n", 
	    __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }
      
   }

   double synthBeamsizeRads = AtaInformation::ataBeamsizeRadians(
      maxSkyFreqMhz, synthBeamsizeAtOneGhzArcSec_);

   // Try to reuse the current secondary targets
   TargetId prevChosenTargetId(firstChosenTargetId_);
   TargetIdSet::iterator currentSecondaryTargetIter = 
      currentSecondaryTargetsInUse_.begin();
   for (int i = 0; i< nTargetsToChoose; ++i)
   {
      double minSepRads = synthBeamsizeRads * minTargetSeparationBeamsizes;
      cullByMinTargetSeparation(culledTargetMap_,
                                prevChosenTargetId, minSepRads);

      if (culledTargetMap_.size() < 1)
      {
	 throw SseException(
	    "no targets remain after culling by min target separation\n", 
	    __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }

      TargetMap::const_iterator bestTargetIter;
      if (currentSecondaryTargetIter != currentSecondaryTargetsInUse_.end())
      {
	 // try to reuse the current secondary target
	 TargetId currentSecondaryTarget = *currentSecondaryTargetIter++;
	 bestTargetIter = bestObsRise(culledTargetMap_, currentSecondaryTarget);
      }
      else 
      {
	 // grab the first (ie highest weighted) target on the list
	 bestTargetIter = bestObsRise(culledTargetMap_);
      }

      if (bestTargetIter == culledTargetMap_.end())
      {
	 throw SseException(
	    "no targets remaining to choose from\n", __FILE__, __LINE__,
	    SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
      }

      TargetId nextTargetId = (*bestTargetIter).first;
      chosenTargetIds.insert(nextTargetId);

      prevChosenTargetId = nextTargetId;

      VERBOSE2(getVerboseLevel(), methodName 
	       << "chose target " << nextTargetId << endl;);

   }

   // save secondary targets for next iteration of target selections
   currentSecondaryTargetsInUse_ = chosenTargetIds; 

}

TargetId OrderedTargets::determinePrimaryTargetId(
   TargetMap & targetMap, TargetId targetId)
{
   if (usePreselectedPrimaryFovCenter_)
   {
      return NoPrimaryTargetId;
   }

   // make sure target is in the map
   Assert(targetMap.find(targetId) != targetMap.end());
   Target * target = targetMap[targetId];
   
   // If the target has a valid associated primaryTargetId then
   // return it, else return the targetId itself as the primary target id.

   TargetId primaryTargetId = target->getPrimaryTargetId();
   if (primaryTargetId > 0)
   {
      return primaryTargetId;
   }
   
   return targetId;

}

void OrderedTargets::printTargetInfoInSystemlog(
   TargetId targetId, Target *target, const ObsRange &obsRange)
{
   stringstream strm;
   strm
      << "Next 'first' target: " << targetId 
      << " (" << target->getCatalog()  << ")" << endl 
      << "Needed frequencies:  " <<  obsRange << " MHz" << endl 
      << "Rise Time: " << SseUtil::isoDateTime(target->getRiseTimeUtc()) << endl
      << "Set Time : " << SseUtil::isoDateTime(target->getSetTimeUtc()) << endl;

   // lmst:
   int raHours;
   int raMin;
   double raSec;
   SseAstro::decimalHoursToHms(SseAstro::radiansToHours(lmstRads_), 
                        &raHours, &raMin, &raSec);
   
   strm
      << "LMST: "
      << setfill('0')
      << SseAstro::radiansToHours(lmstRads_) << " hours // "
      << setw(2) << raHours << ":"
      << setw(2) << raMin << ":"
      << setw(2) << raSec << " hms"
      << endl;

   SseArchive::SystemLog() << strm.str();
}



// Search the Target list and select best target.
// Log Rise/Set Times in system log.

void OrderedTargets::getBestTarget(
   TargetMap & targetMap, // list of targets to choose from
   TargetId currentTargetId,     // target currently being observed
   bool activitiesInProgress,    // are activities currently running?
   TargetId & bestTargetId,      // return: choosen target
   ObsRange & bestTargetObsRange // frequencies needed for this target
   )
{
   VERBOSE2(getVerboseLevel(), "OrderedTargets::getBestTarget" <<
	    " current targetId:  " <<  currentTargetId << endl;);    

   TargetMap::const_iterator best;
   if (currentTargetId == 0)
   {
      best = bestObsRise(targetMap);
      bestTargetId = (*best).first;
   }
   else
   {
      best = bestObsRise(targetMap, currentTargetId);
      bestTargetId = (*best).first;
      if ((bestTargetId != currentTargetId) 
	  && activitiesInProgress && waitTargetComplete_)
      {
	 throw ActivityStartWaitingForTargetComplete(
	    "Target change must wait until current target is done.\n");
      }
   }

   Target *target = (*best).second;
   bestTargetObsRange = target->unobserved();

   stringstream logMsg;

   logMsg << "Next 'first' target is " << bestTargetId
          << " (" << target->getCatalog() << ")" << endl
          << "Needed frequencies are :  " << bestTargetObsRange << endl;

   VERBOSE2(getVerboseLevel(), logMsg.str());

   cout << logMsg.str();

   AssertMsg(! bestTargetObsRange.isEmpty(),
	     "No frequencies left to observe on chosen target");
   
   printTargetInfoInSystemlog(bestTargetId, target, bestTargetObsRange);

}

int OrderedTargets::getVerboseLevel() const
{
   return verboseLevel_;
}

int OrderedTargets::setVerboseLevel(int level)
{
   verboseLevel_ = level;
   return verboseLevel_;
}


// determine the minimum amount of time (expressed in radians)
// that the target must be visible before it sets.
// Allow enough time for for the minimum number of 
// followup observations to run.

double OrderedTargets::getMinRemainingTimeOnTargetRads() const
{
   return minRemainingTimeOnTargetRads_;
}

double OrderedTargets::computeMinRemainingTimeOnTargetRads() const
{
   // assume overall time for an activity is 2 times the observation length
   // TBD: take into account other activity startup overhead.

   double SecsPerHour = 3600;

   // allow for data collect & signal detect:
   int totalActivityLengthSec = 2 * static_cast<int>(obsLengthSec_); 
   double minRemainingTimeHours = totalActivityLengthSec/SecsPerHour * 
      minNumberReservedFollowupObs_;

   double minRemainingTimeRads = SseAstro::hoursToRadians(minRemainingTimeHours);

   return minRemainingTimeRads;
}


// TBD: Is it still ok to use the culledTargetMap for this?
  
ostream &operator<<(ostream& os,
		    const OrderedTargets& orderedTargets)
{
   vector<OrderedTargets::TargetMap::const_iterator> sortTargets;
   for (OrderedTargets::TargetMap::const_iterator index =
	   orderedTargets.culledTargetMap_.begin();
	index != orderedTargets.culledTargetMap_.end(); index++)
   {
      sortTargets.push_back(index);
   }
   TargetSort targetSort;

   // Sort on Overall Merit
   sort(sortTargets.begin(), sortTargets.end(), targetSort);
   for (vector<OrderedTargets::TargetMap::const_iterator>::const_iterator i =
	   sortTargets.begin(); i != sortTargets.end(); i++)
   {
      Target* target = (*(*i)).second;
      os << *target;
      os << endl;
   }
   return os;
}



TargetSort::TargetSort()
{
}

bool TargetSort::operator()(const OrderedTargets::TargetMap::const_iterator &first,
			    const OrderedTargets::TargetMap::const_iterator &second) const
{
// TBD change to use TargetMerit

   return
      ((*first).second->overallMerit() >
       (*second).second->overallMerit());
}
