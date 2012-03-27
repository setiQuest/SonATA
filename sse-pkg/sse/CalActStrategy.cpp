/*******************************************************************************

 File:    CalActStrategy.cpp
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

#include "CalActStrategy.h" 
#include "ActivityWrappers.h"
#include "ActParameters.h"
#include "DbParameters.h"
#include "TscopeParameters.h"
#include "SchedulerParameters.h"
#include "ArrayLength.h"
#include "CalTargets.h"
#include "SseAstro.h"
#include "SseArchive.h"
#include "Interpolate.h"
#include "MysqlQuery.h"
#include <sstream>

using namespace std;

CalActStrategy::CalActStrategy(
                               Scheduler *scheduler,
                               Site *site, 
                               const NssParameters &nssParameters,
                               int verboseLevel):
   ActivityStrategy(scheduler, site, nssParameters, verboseLevel),
   nssParameters_(nssParameters),
   origTuneOffsetMhz_(nssParameters_.tscope_->getBasebandTuneOffsetMhz())
{
   // Get information about calibrators from database
   if (! nssParameters_.db_->useDb())
   {
      throw SseException( 
	 "automatic calibration target choice does not work when database is off\n",  
	 __FILE__, __LINE__, SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
   }
  
}

CalActStrategy::~CalActStrategy()
{
}

NssParameters & CalActStrategy::getNssParameters()
{
   return nssParameters_;
}

void CalActStrategy::startInternalHook()
{
   loadCalFreqListMhz();

   calTargets_.loadTargetsFromDb(nssParameters_.db_->getDb(), getCatalogName());
   calTargets_.computeFluxAtFreq(getNextCalFreqMhz());

   prepareParameters();
}

void CalActStrategy::addCalFreqMhz(double freqMhz)
{
   calFreqQueueMhz_.push_back(freqMhz);
}


double CalActStrategy::getNextCalFreqMhz()
{
   if (calFreqQueueMhz_.empty())
   {
      throw SseException("No more cal freqs available\n");
   }

   return calFreqQueueMhz_.front();
}

struct TargetValues
{
   double fluxJy;
   double sunAngleRads;
   bool isVisible;
};

void CalActStrategy::selectCalTarget(TargetId &targetId, double &targetFluxJy)
{
   const string methodName(" CalActStrategy::selectCalTarget");
   /*
     Find the cal target with the max flux at the
     (already selected) cal freq, that's up at least the min amount of time,
     and that is far enough away from the sun.
   */
   const double minUpTimeForCalHours(0.5); // tbd get from params

   const vector<TargetInfo> & targetInfoVect(calTargets_.getTargetInfo());
   TscopeParameters * tscope(nssParameters_.tscope_);

   // default to current time
   time_t obsTime;
   time(&obsTime);

   double sunRaRads;
   double sunDecRads;
   SseAstro::sunPosition(obsTime, &sunRaRads, &sunDecRads);

   double sunAvoidAngleDeg(30);  // TBD make a cal param
   double sunAvoidAngleRads(SseAstro::degreesToRadians(sunAvoidAngleDeg));

   VERBOSE2(getVerboseLevel(), methodName
	    << " sun Ra (Hrs), Dec (deg) = "
            << SseAstro::radiansToHours(sunRaRads)
	    << " " << SseAstro::radiansToDegrees(sunDecRads)
            << ", avoid angle (deg) = " 
            << sunAvoidAngleDeg << endl;);

   double minFluxJy(5);  // Per Peter Backus, 17 July 2009
   vector<TargetValues> targetValueVect;
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

      double targetToSunSepRads = SseAstro::angSepRads(
         SseAstro::hoursToRadians(targetInfoVect[i].ra2000Hours),
         SseAstro::degreesToRadians(targetInfoVect[i].dec2000Deg),
         sunRaRads, sunDecRads);

      TargetValues values;
      values.fluxJy = targetInfoVect[i].fluxJy;
      values.sunAngleRads = targetToSunSepRads;
      values.isVisible = false;
      if ((untilSetHours > minUpTimeForCalHours) && (values.fluxJy >= minFluxJy))
      {
         values.isVisible = true;
      }
      targetValueVect.push_back(values);
   }

   // First try to pick the strongest visible target that's outside
   // the sun avoidance angle

   int bestTargetIndex(-1);
   double maxFluxAtCalFreq(-1);
   for (unsigned int i=0; i<targetValueVect.size(); ++i)
   {
      TargetValues &values(targetValueVect[i]);
      if (values.isVisible)
      {
         if (values.sunAngleRads > sunAvoidAngleRads)
         {
            if (values.fluxJy > maxFluxAtCalFreq)
            {
               maxFluxAtCalFreq = values.fluxJy;
               bestTargetIndex = i;
            }
         }
      }
   }

   if (bestTargetIndex < 0)
   {
      // No targets met all the constraints.
      // This time try to pick the source farthest from 
      // the sun that's available regardless of flux,
      // even if it's within the sun avoidance angle.

      double maxSunAngleRads(-1);
      for (unsigned int i=0; i<targetValueVect.size(); ++i)
      {
         TargetValues &values(targetValueVect[i]);
         if (values.isVisible)
         {
            if (values.sunAngleRads > maxSunAngleRads)
            {
               maxSunAngleRads = values.sunAngleRads;
               bestTargetIndex = i;
            }
         }
      }
   }

   if (bestTargetIndex < 0)
   {
      throw SseException("No cal targets match specified freq, site info, "
                         + string("sun avoid angle & minimum uptime\n"),
                         __FILE__, __LINE__ );
   }

   const TargetInfo & target(targetInfoVect[bestTargetIndex]);

   SseArchive::SystemLog() 
	   << getCalType()
      << " Cal target: " 
      << target.targetId
      << " (" << target.name
      << "), estimated flux: " << target.fluxJy << " Jy" 
      << " @ " << getNextCalFreqMhz() << " MHz, " 
      << "sun angle deg: "
      << SseAstro::radiansToDegrees(targetValueVect[bestTargetIndex].sunAngleRads)
      << endl;

   targetId = target.targetId;
   targetFluxJy = target.fluxJy;
}

void CalActStrategy::prepareParameters()
{
   string methodName("CalActStrategy::getNextActivity");

   // set activity type (for logging purposes)
   string calActType("cal");
   if (! nssParameters_.act_->setActivityType(calActType))
   {
      throw SseException("tried to set invalid activity type: " + calActType
                         + " in " + methodName + "\n", __FILE__, __LINE__);
   }    

   // cal type 
   if (! nssParameters_.tscope_->setCalType(getCalType()))
   {
      throw SseException("tried to set invalid cal type: " + getCalType()
                         + " in " + methodName + "\n", __FILE__, __LINE__);
   }    

   // number of cal cycles
   if (! nssParameters_.tscope_->setCalNumCycles(getNumCalCycles()))
   {
      throw SseException("tried to set invalid number of cal cycles: " 
                         + SseUtil::intToStr(getNumCalCycles())
                         + " in " + methodName + "\n", __FILE__, __LINE__);
   }    

   // select cal target, determine integration time
   TargetId targetId;
   double targetFluxJy;
   selectCalTarget(targetId, targetFluxJy);

   int calTimeSecs(getCalTimeSecs(getNextCalFreqMhz(), targetFluxJy));

   if (! nssParameters_.tscope_->setCalTimeSecs(calTimeSecs))
   {
      throw SseException("tried to set invalid cal time: " 
                         + SseUtil::intToStr(calTimeSecs)
                         + " in " + methodName + "\n", __FILE__, __LINE__);
   }    

   // set calibrator targetId on all beams (synth & primary)
   // It's ok to set target id on beams that won't be used.
   // TBD: get beam names from ActParameters
   const char *beams[] = {"beam1", "beam2", "beam3", "beam4", 
                          "beam5", "beam6", "primary" };

   for (int i=0; i<ARRAY_LENGTH(beams); ++i)
   {
      nssParameters_.act_->setTargetIdForBeam(beams[i], targetId);
   }

   /*
     Force primary beam selection by targetid, not coords.
    */
   nssParameters_.act_->setPrimaryBeamPositionType(
       ActParameters::PRIMARY_BEAM_POS_TARGET_ID);

   /*
     Set rf tune to user in scheduler so that parameters get used.
     This isn't strictly necessary since the dxs are ignored in the
     cal activity, but should do no harm, and helps document the tuning mode.
     TBD: get user mode name from SchedParameters
   */
   string rfTuneUser("user");
   if (! nssParameters_.sched_->setRfTune(rfTuneUser))
   {
      throw SseException("tried to set invalid rf tune type: " + rfTuneUser
                         + " in " + methodName + "\n",__FILE__,__LINE__);
   }    

   /*
     Set the tscope baseband tune offset to zero, so that
     the specified cal freq is exactly what is used, to make it
     easier to avoid RFI regions.
   */
   double tuneOffsetMhz(0);
   if (! nssParameters_.tscope_->setBasebandTuneOffsetMhz(tuneOffsetMhz))
   {
      throw SseException(string("tried to set invalid tune offset") 
                         + " in " + methodName + "\n", __FILE__, __LINE__);
   }    

}


void CalActStrategy::setTuningFreqInParams(double freqMhz)
{
   string methodName("CalActStrategy::setTuningFreqInParams");

   // Set cal freq on all tunings (this should do no harm for tunings
   // that are not enabled).
   // TBD get tuning names from TscopeParameters

   const char *tunings[] = {"tuninga", "tuningb", "tuningc", "tuningd"};
   for (int i=0; i<ARRAY_LENGTH(tunings); ++i)
   {
      if (! nssParameters_.tscope_->setTuningSkyFreqMhz(
             tunings[i], freqMhz))
      {
         throw SseException("error trying to set skyfreq on tscope tuning: " 
                            + string(tunings[i]) + " in " + methodName + "\n",
                            __FILE__,__LINE__);
      }
   }
}

void CalActStrategy::logRemainingCalFreqs()
{
   stringstream strm;

   strm << "Remaining cal freqs (MHz): ";

   for (unsigned int i=0; i< calFreqQueueMhz_.size(); ++i)
   {
      strm << calFreqQueueMhz_[i] << " ";
   }
   strm << endl;

   SseArchive::SystemLog() << strm.str();

}

Activity * CalActStrategy::getNextActivity(
   NssComponentTree *nssComponentTree)
{
   logRemainingCalFreqs();

   setTuningFreqInParams(getNextCalFreqMhz());

   Activity *act = NewCalibrateActWrapper(
      getNextActId(), this,
      nssComponentTree, nssParameters_,
      getVerboseLevel());

   return act;
}


bool CalActStrategy::moreActivitiesToRun() 
{
   if (calFreqQueueMhz_.empty())
   {
      throw SseException("No more cal freqs available\n");
   }

   calFreqQueueMhz_.pop_front();
   
   return(! calFreqQueueMhz_.empty());
}

bool CalActStrategy::okToStartNewActivity()
{
   // This needs to be true for the strategy to wrap up

   return true;
}

double CalActStrategy::getOrigTuneOffsetMhz()
{
   return origTuneOffsetMhz_;
}

/*
  Load the calibration fluxes and integration times from the specified table
  whose frequencies contain obsFreqMhz.
 */

void CalActStrategy::loadCalIntegrationTimes(const string &dbTableName, double obsFreqMhz,
                                             InterpolateLinear &interpCalTimeSecs)
{
   stringstream queryStrm;
   
   queryStrm << "select fluxJy, timeSecs "
             << "from " << dbTableName
             << " where "
             << obsFreqMhz << " >= lowFreqMhz and "
             << obsFreqMhz << " <= highFreqMhz";
   
   enum resultCols {fluxJyCol, timeSecsCol, numCols};
   
   MysqlQuery query(nssParameters_.db_->getDb());
   query.execute(queryStrm.str(), numCols, __FILE__, __LINE__);

   bool foundData(false);
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      double fluxJy(query.getDouble(row, fluxJyCol, 
                                    __FILE__, __LINE__));
      
      double timeSecs(query.getDouble(row, timeSecsCol, 
                                    __FILE__, __LINE__));

      interpCalTimeSecs.addValues(fluxJy, timeSecs);

      foundData = true;
   }

   /*
     Extend the min and max times to cover the full
     range of possible fluxes for "extrapolated" lookups.
    */

   if (foundData)
   {
      double minFluxJy(1);
      interpCalTimeSecs.addValues(minFluxJy, interpCalTimeSecs.maxY());

      double maxFluxJy(10000);
      interpCalTimeSecs.addValues(maxFluxJy, interpCalTimeSecs.minY());
   }
}

int CalActStrategy::getPhaseCalTimeSecs(double obsFreqMhz,
                                             double targetFluxJy)
{
   const string BfCalTimeTable("BfCalPhaseTime");
   return computeCalTimeSecs(BfCalTimeTable, obsFreqMhz, targetFluxJy);
}

int CalActStrategy::computeCalTimeSecs(const string & BfCalTimeTable,
                                       double obsFreqMhz, double targetFluxJy)
{
   // Create lookup table of integration time in secs, indexed by fluxJy.
   InterpolateLinear interpCalTimeSecs;
   loadCalIntegrationTimes(BfCalTimeTable, obsFreqMhz, interpCalTimeSecs);

   VERBOSE2(getVerboseLevel(), BfCalTimeTable << ": "
            << "obsFreqMhz = " << obsFreqMhz << " MHz "<< endl
            << "fluxJy timeSecs" << endl
            << interpCalTimeSecs << endl;);
   
   double calTimeSecs(-1);
   if (interpCalTimeSecs.inter(targetFluxJy, calTimeSecs))
   {
      return static_cast<int>(calTimeSecs);
   }

   SseArchive::SystemLog() 
      << "CalActStrategy: no integration time data is available for sources at "
      << obsFreqMhz << " MHz in database table " << BfCalTimeTable << ".  "
      << "Using tscope parameter value." << endl;
   
   return getNssParameters().tscope_->getCalTimeSecs();
}

void CalActStrategy::activityCompleteInternalHook(
   Activity *activity, bool failed)
{
   if (failed)
   {
      /*
        Retry the same cal freq.  Do this by adding a dummy 
        freq to the queue that will get discarded in the
        restart process instead of the failed freq.
      */
      double dummyFreqMhz(0);
      calFreqQueueMhz_.push_front(dummyFreqMhz);
   }
   else
   {
	   if (getCalType().compare(0,5,"delay") == 0)
	   {
		   stringstream summaryScript;
		   summaryScript << getenv("HOME") << "/sonata_install/bin/obs-delay-cal-report.sh";
		   system(summaryScript.str().c_str());
	   }
   }
}
