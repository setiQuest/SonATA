/*******************************************************************************

 File:    Target.cpp
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

// Hold information about individual targets.

#include "Target.h"
#include "Angle.h"
#include "Assert.h"
#include "SseUtil.h"
#include "SseAstro.h"
#include <ctime>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sstream>

static const double DefaultCatalogPriorityMerit = 0.5;

using namespace std;

Target::Target(RaDec raDec,
               double pmRaMasYr,
               double pmDecMasYr,
	       double parallaxMas,
	       TargetId targetId,
	       TargetId primaryTargetId,
	       const string & catalog, 
	       const ObsRange& desiredObservingFreqRangesMhz,
	       double maxDistLightYears,
	       SiteView * siteView,
	       double obsLengthSec,
	       float64_t minBandwidthMhz,
	       double minRemainingTimeOnTargetRads
   ):
   targetPosition_(raDec, pmRaMasYr, pmDecMasYr, parallaxMas),
   setiTargetId_(targetId),
   primaryTargetId_(primaryTargetId),
   catalog_(catalog),
   maxDistLightYears_(maxDistLightYears),
   siteView_(siteView),
   obsLengthSec_(obsLengthSec),
   minUnobsSegmentBwMhz_(minBandwidthMhz),
   minRemainingTimeOnTargetRads_(minRemainingTimeOnTargetRads),
   lmstRads_(0),
   obsDate_(0),
   alreadyCompletelyObserved_(false),
   unobservedUpToDate_(false),
   timeLeftMerit_(0),
   meritCalculated_(false),
   beforeSet_(0),
   riseTimeUtc_(0),
   setTimeUtc_(0),
   catalogPriorityMerit_(DefaultCatalogPriorityMerit),
   preferredPrimaryTargetId_(-1),
   isVisible_(false),
   timeSinceRiseRads_(0),
   timeUntilSetRads_(0)
{
   desiredFreqRange_ = desiredObservingFreqRangesMhz; 
}

void Target::setTime(double lmstRads, time_t obsDate)
{
   meritCalculated_ = false;

   lmstRads_ = lmstRads;
   obsDate_ = obsDate;

   timeLeftMerit_ = 0;
   beforeSet_ = 0;
   riseTimeUtc_ = 0;
   setTimeUtc_ = 0;
   isVisible_ = false;
   timeSinceRiseRads_ = 0;
   timeUntilSetRads_ = 0;

   setVisibility();
   computeOverallMerit();
}

/*
  Catalog merit:

  Is the catalog on the high or low priority list,
  and what's its position (order) on that list.
  The first catalog gets the most weight.
 */

void Target::setCatalogPriority(CatalogPriority priority, int order)
{
   meritCalculated_ = false;

   if (priority == CatalogHighPriority)
   {
      if (order == 0)
      {
         catalogPriorityMerit_ = 100000;
      }
      else
      {
         catalogPriorityMerit_ = 1000;
      }
   }
   else
   {
      Assert(priority == CatalogLowPriority);

      if (order == 0)
      {
         catalogPriorityMerit_ = 10;
      }
      else
      {
         catalogPriorityMerit_ = 0.5;
      }
   }
}

void Target::setPreferredPrimaryTargetId(TargetId targetId)
{
   preferredPrimaryTargetId_ = targetId;
}

RaDec Target::getPosition() const
{
   // get target position in coord of date
   RaDec raDec = targetPosition_.positionAtNewEpochAndEquinox(obsDate_); 

   return raDec;
};

RaDec Target::getRaDec() const
{
   return targetPosition_.getJ2000RaDec();
};

string Target::getCatalog() const
{
   return catalog_;
}

time_t Target::getTime() const
{
   return obsDate_;
};

bool Target::isVisible() const
{
   return isVisible_;
}

double Target::getTimeSinceRiseRads() const
{
   return timeSinceRiseRads_;
}

double Target::getTimeUntilSetRads() const
{
   return timeUntilSetRads_;
}

time_t Target::getRiseTimeUtc() const
{
   return riseTimeUtc_;
}

time_t Target::getSetTimeUtc() const
{
   return setTimeUtc_;
}

void Target::setVisibility()
{
   // get target position in coord of date
   RaDec raDec = targetPosition_.positionAtNewEpochAndEquinox(obsDate_);  

   // determine if target is visible, and if so,
   // how long it's been up, and how long until it sets

   isVisible_ = siteView_->isTargetVisible(
      lmstRads_, raDec, timeSinceRiseRads_, timeUntilSetRads_);

   if (isVisible_)
   {
      computeRiseSetTimes();
   }
}

void Target::computeRiseSetTimes()
{
   riseTimeUtc_ = obsDate_ - 
      SseAstro::siderealRadsToSolarTimeSecs(timeSinceRiseRads_);

   setTimeUtc_ = obsDate_ + 
      SseAstro::siderealRadsToSolarTimeSecs(timeUntilSetRads_);
}


double Target::timeLeftMerit() const
{
   return timeLeftMerit_;
}

/*
  Target must be visible for at least the minimum amount of
  time.
 */
void Target::computeTimeLeftMerit()
{
   if (! isVisible())
   {
      timeLeftMerit_ = 0;
      return;
   }

   Assert(timeUntilSetRads_ > 0);
   if (timeUntilSetRads_ - minRemainingTimeOnTargetRads_ < 0.0)
   {
      timeLeftMerit_ = 0;
      return;
   }

   timeLeftMerit_ = 1;
   return;
}

double Target::overallMerit() 
{
   if (!meritCalculated_)
   {
      computeOverallMerit();
   }
   return overallMerit_;
}

// This routine is obsolete
// It has been replace by TargetMerit::overallMerit()
//
void Target::computeOverallMerit()
{
   computeTimeLeftMerit();
   
#ifdef OriginalMerit

   overallMerit_ = catalogMerit() * obsMerit() * 
      completelyObservedMerit() * distMerit() *
      nearMeridianMerit() * decMerit() * 
      timeLeftMerit(); 
#else

   // simplified merit to force lower dec observations
   // and more uniform galactic center survey coverage

   overallMerit_ = catalogMerit() *
      completelyObservedMerit() *
      decMerit() *
      timeLeftMerit();

#endif   

   meritCalculated_ = true;
   
}

double Target::catalogMerit() const
{
   return catalogPriorityMerit_;
}

double Target::obsMerit() const
{
   // value ranges from 0 to 10
   // (targets already partially observed get higher priority)

   double merit(0);
   const double maxValue(10.0);
   float64_t fractionObserved(this->fractionObserved());

   if (fractionObserved >= 1.0)
   {
      merit = 0.0;
   }
   else
   {
      merit = 1.0 / (1.0 - fractionObserved);
      if (merit > maxValue)
      {
	 merit = maxValue;
      }
   }
   return merit;
}

double Target::distMerit() const
{
   const double DREHER_DIST_CONST =256.0;

   // TBD rework this so the falloff is much more gradual,
   // and never goes below 1.

   double parallaxArcSec = targetPosition_.getParallaxArcSec();

#if 0
   double lightYears = targetPosition_.distanceLightYears();
   if (lightYears > maxDistLightYears_) 
   {
      return 0.0;
   }
   else
#endif

   {
      double merit = max(1.0, parallaxArcSec * parallaxArcSec * DREHER_DIST_CONST);
      return merit;
   }

}


double Target::completelyObservedMerit() const
{
   // Already observed targets get very low merit so that
   // they are only chosen if no unobserved targets are available.

   const double alreadyObservedMerit(0.001);  
   const double notAlreadyObservedMerit(1.0);

   if (alreadyCompletelyObserved_)
   {
      return alreadyObservedMerit;
   }

   return notAlreadyObservedMerit;
}

// Prefer targets that are at lower dec limits.
// Returned merit value ranges from 1 for north pol to 
// ~69 at the south pol.

double Target::decMerit() const
{
   RaDec raDec = getRaDec();

   double maxDecRads = M_PI/2;
   double merit = maxDecRads - raDec.dec + 1.0;
   merit *= merit * merit;

   return merit;
}

/*
  Targets that share the preferredPrimaryTargetId get
  priority.
 */
double Target::primaryTargetIdMerit() const
{
   double merit(1);

   if ((preferredPrimaryTargetId_ > 0) &&
      (primaryTargetId_ == preferredPrimaryTargetId_))
   {
      merit = 10;
   }

   return merit;
}



// Prefer targets that are near the meridian,
// within the specified rise & set cutoffs.
// Rising side gets higher merit than setting side.
// Values range from 100 - 200.

double Target::nearMeridianMerit() const
{
   const double riseSideMerit(200.0);
   const double meritStep = 50;
   const double setSideMerit(riseSideMerit - meritStep);
   const double outsideMeridianWindowMerit(setSideMerit - meritStep);

   const double riseSideCutoffRads(SseAstro::hoursToRadians(2.0));
   const double setSideCutoffRads(SseAstro::hoursToRadians(1.0));

   double targetRaRads = getRaDec().ra.getRadian();

   // see if target is within the rise-side window of the meridian
   double riseDiffRads = targetRaRads - lmstRads_;
   if (riseDiffRads < 0)
   {
      riseDiffRads += (2 * M_PI);
   }
   if (riseDiffRads <= riseSideCutoffRads)
   {
      // score higher merit nearer meridian
      double percentDistFromMeridian = riseDiffRads / riseSideCutoffRads;
      double merit = riseSideMerit - (meritStep * percentDistFromMeridian);

      return merit;
   }

   // see if target is within the set-side window of the meridian
   double setDiffRads = lmstRads_ - targetRaRads;
   if (setDiffRads < 0)
   {
      setDiffRads += (2 * M_PI);
   }
   if (setDiffRads <= setSideCutoffRads)
   {
      // score higher merit nearer meridian
      double percentDistFromMeridian = setDiffRads / setSideCutoffRads;
      double merit = setSideMerit - (meritStep * percentDistFromMeridian);

      return merit;
   }

   return outsideMeridianWindowMerit;
}

void Target::addObserved(float64_t lowFreq, float64_t highFreq)
{
   observedFreqRange_.addInOrder(lowFreq, highFreq);
   unobservedUpToDate_ = false;
}

  
void Target::addObservedOutOfOrder(float64_t lowFreq, float64_t highFreq)
{
   observedFreqRange_.addOutOfOrder(lowFreq, highFreq);
   unobservedUpToDate_ = false;
}

void Target::removeObserved(float64_t lowFreq, float64_t highFreq)
{
   observedFreqRange_ = observedFreqRange_ - Range(lowFreq, highFreq);
   unobservedUpToDate_ = false;
}
  
TargetId Target::getTargetId() const
{
   return setiTargetId_;
}

TargetId Target::getPrimaryTargetId() const
{
   return primaryTargetId_;
}

void Target::printSeqs(ostream& os) const
{
   calculateUnobserved();
   os << observedFreqRange_ << '/' << unobservedFreqRange_;
}

ObsRange Target::unobserved() const
{
   calculateUnobserved();
   return unobservedFreqRange_;
}

ObsRange Target::observed() const
{
   return observedFreqRange_;
}
 
bool Target::hasRangeGt(float64_t minBandwidth) const
{
   calculateUnobserved();
   return unobservedFreqRange_.hasRangeGt(minBandwidth);
}


double Target::totalRangeGt(float64_t minBandwidth) const
{
   calculateUnobserved();
   return unobservedFreqRange_.totalRangeGt(minBandwidth);
}

void Target::calculateUnobserved() const
{
   if (! unobservedUpToDate_)
   {
      unobservedFreqRange_ = desiredFreqRange_ - observedFreqRange_;
      unobservedFreqRange_.removeRangesLtWidth(minUnobsSegmentBwMhz_);
      unobservedUpToDate_ = true;
   }
}

float64_t Target::fractionObserved() const
{
   //cerr << "Observed " << observedFreqRange_.totalRange() << endl;
   //cerr << "Desired " << desiredFreqRange_.totalRange() << endl;
   return observedFreqRange_.totalRange()/desiredFreqRange_.totalRange();
}


// Set the desiredFreqRange and observedFreqRange back to the
// values they had at object creation time.

void Target::resetObservations() 
{
   observedFreqRange_.removeAllRanges();
   unobservedUpToDate_ = false;
   meritCalculated_ = false;
}

void Target::setAlreadyCompletelyObserved(bool value)
{
   meritCalculated_ = false;
   alreadyCompletelyObserved_ = value;
}

bool Target::isAlreadyCompletelyObserved()
{
   return alreadyCompletelyObserved_;
}


ostream &operator<<(ostream& os, const Target &target)
{
   RaDec raDec = target.getRaDec();

   // Target id
   os << setw(5) << target.setiTargetId_;

   // target type
   os << setw(4) << target.catalog_;  

   os.width(3);
   os.precision(2);
   os.setf(ios::fixed);
  
   // Position
   os << " " << raDec;
  
   // distance
   os << "  " << setw(6) << target.targetPosition_.distanceLightYears();

   // merit
   os.precision(2);
   os << " " << setw(8) << (const_cast<Target &>(target)).overallMerit();

   // Hour Angle Rise
   //os << ' ' <<  // TBD

   // current hour angle
   double hour_angle = target.lmstRads_ - raDec.ra;
   os << "  " << Hm(Radian(hour_angle));

   // rise/set times
   os << "    " << SseUtil::isoTimeWithoutTimezone(target.getRiseTimeUtc()) 
      << "/" << SseUtil::isoTimeWithoutTimezone(target.getSetTimeUtc());

   // freqs
   double maxFreq = target.observedFreqRange_.maxValue();
   if (maxFreq > 0.0)
   {
      os << "   " << setprecision(1) << maxFreq;
   }
   //target.printSeqs(os);

   return os;
  
}
