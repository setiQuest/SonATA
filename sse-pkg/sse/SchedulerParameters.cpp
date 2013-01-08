/*******************************************************************************

 File:    SchedulerParameters.cpp
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

// Holds scheduler related parameters

#include <ace/OS.h>            // for SwigScheduler.h
#include "SchedulerParameters.h"
#include "RangeParameter.h"
#include "ChoiceParameter.h"
#include "MultiChoiceParameter.h"
#include "AnyValueParameter.h"
#include "AtaInformation.h"
#include "TargetMerit.h"
#include "Assert.h"

static const char *DbTableName="SchedulerParameters";
static const char *IdColNameInActsTable="schedParametersId";
static const char *ChoiceOn = "on";
static const char *ChoiceOff = "off";
static const char *ChoiceWarn = "warn";
static const char *TuneDxsOpts[] = {"range", "user", "forever"};
static const char *ChooseTargetOpts[] = {"user", "semiauto", "auto", 
                                         "autorise", "commensal"};

enum RfTuneOptions { RF_TUNE_AUTO, RF_TUNE_USER };
static const char *RfTuneOpts[] = {"auto", "user"};

enum FollowupModeOptions { FOLLOWUP_MODE_AUTO, FOLLOWUP_MODE_USER };
static const char *FollowupModeOpts[] = {"auto", "user"};

using namespace std;
typedef SchedulerParameters Param;

struct SchedulerParametersInternal
{
protected:
   float64_t defaultBeginSkyFreqMhz;
   float64_t defaultEndSkyFreqMhz;
public:

   // methods:
   SchedulerParametersInternal();  
   // default copy constructor and assignment operator are safe

   ChoiceParameter<string> rfTune;
   ChoiceParameter<string> dxTune;
   ChoiceParameter<string> target;
   RangeParameter<float64_t> beginObsFreqMhz;
   RangeParameter<float64_t> endObsFreqMhz;
   ChoiceParameter<string> pipelining;
   ChoiceParameter<string> followup;
   ChoiceParameter<string> followupMode;
   RangeParameter<float64_t> dxRound;
   RangeParameter<float64_t> dxOverlap;
   RangeParameter<float64_t> dxTuningTolerance;
   RangeParameter<uint32_t> minNumberReservedFollowupObs;
   ChoiceParameter<string> useBeam1;
   ChoiceParameter<string> useBeam2;
   ChoiceParameter<string> useBeam3;
   ChoiceParameter<string> useBeam4;
   ChoiceParameter<string> useBeam5;
   ChoiceParameter<string> useBeam6;
   RangeParameter<double> maxTargetDistLightYears;
   RangeParameter<double> minDxPercentBwToUse;
   ChoiceParameter<string> multipleTargets;
   RangeParameter<double> sunAvoidAngleDeg;
   RangeParameter<double> moonAvoidAngleDeg;
   RangeParameter<double> geosatAvoidAngleDeg;
   RangeParameter<double> zenithAvoidAngleDeg;
   RangeParameter<double> autoRiseTimeCutoffMinutes;
   RangeParameter<double> minTargetSepBeamsizes;
   ChoiceParameter<string> waitTargetComplete;
   RangeParameter<double> decLowerLimitDeg;
   RangeParameter<double> decUpperLimitDeg;
   RangeParameter<int> repeatCount;
   RangeParameter<int> maxActFailures;
   RangeParameter<int> pauseTimeBetweenActRestartsSecs;
   RangeParameter<int> tscopeReadyMaxFailures;
   RangeParameter<int> tscopeReadyPauseSecs;
   ChoiceParameter<string> sendEmailOnStrategyFailure;
   AnyValueParameter<string> strategyFailureEmailAddressList;
   ChoiceParameter<string> stopOnStrategyFailure;
   ChoiceParameter<string> checkTargets;
   RangeParameter<int> targetAvailActSetupTime;
   RangeParameter<double> beamBandwidthMhz;
   RangeParameter<int> primaryTargetIdCountCutoff;
   RangeParameter<int> catalogsHighPriorityMaxCounts;
   AnyValueParameter<string> catalogsHighPriority;
   AnyValueParameter<string> catalogsLowPriority;
   ChoiceParameter<string> commensalCalEnabled;
   RangeParameter<double> commensalCalIntervalMinutes;
   RangeParameter<double> commensalCalLengthMinutes;
   ChoiceParameter<string> rotatePrimaryIdsEnabled;
   RangeParameter<double> rotatePrimaryIdsIntervalMinutes;
   MultiChoiceParameter<string> tasks;
   MultiChoiceParameter<string> targetMeritFactors;
};

SchedulerParametersInternal::SchedulerParametersInternal():

   defaultBeginSkyFreqMhz(1755.0),

   defaultEndSkyFreqMhz(3005.0),

   rfTune("rftune", "", "method used to tune RF", 
          RfTuneOpts[RF_TUNE_AUTO]),

   dxTune("dxtune", "", "method used to tune DXs\n\t\t\t\t",
           TuneDxsOpts[Param::TUNE_DXS_USER]),

   target("target", "", "method used to choose targets\n\t\t\t\t",
          ChooseTargetOpts[Param::CHOOSE_TARGET_USER]),

   beginObsFreqMhz("beginfreq", "MHz",
		   "starting frequency for range/auto observations\n\t\t\t\t",
		   defaultBeginSkyFreqMhz, AtaInformation::AtaMinSkyFreqMhz, 
		   AtaInformation::AtaMaxSkyFreqMhz),

   endObsFreqMhz("endfreq", "MHz",
		 "ending frequency for range/auto observations\n\t\t\t\t",
		 defaultEndSkyFreqMhz, AtaInformation::AtaMinSkyFreqMhz, 
		 AtaInformation::AtaMaxSkyFreqMhz),

   pipelining("pipe", "", "should activities be pipelined", ChoiceOn),

   followup("followup", "",
	    "follow up activities when signals are confirmed\n\t\t\t\t",
	    ChoiceOff),

   followupMode("followupmode", "",
		"mode for automatic or manual (user) followup\n\t\t\t\t of activities",
		FollowupModeOpts[FOLLOWUP_MODE_AUTO]),

   dxRound("dxround", "MHz",
	    "for range observations, rounds down to nearest\n\t\t\t\t multiple of dxround",
	    0.0, 0.0, 100.0),

   dxOverlap("dxoverlap", "MHz",
	      "overlap between adjacent DXs",
	      0.0, 0.0, 100),

   dxTuningTolerance("dxtunetol", "MHz",
		      "maximum difference between requested & \n\t\t\t\tactual DX center freq",
		      0.0, 0.0, 100),

   minNumberReservedFollowupObs("minfollowups", "observations", 
				"reserve time for the minimum number of \n\t\t\t\tfollowup observations", 
				12, 0, 100),

   useBeam1("beam1", "", "use beam 1", ChoiceOn),
   useBeam2("beam2", "", "use beam 2", ChoiceOff),
   useBeam3("beam3", "", "use beam 3", ChoiceOff),
   useBeam4("beam4", "", "use beam 4", ChoiceOff),
   useBeam5("beam5", "", "use beam 5", ChoiceOff),
   useBeam6("beam6", "", "use beam 6", ChoiceOff),

   maxTargetDistLightYears("maxdistly", "light years", 
			   "maximum target distance", 
			   225, 4, 1000),

   minDxPercentBwToUse("mindxbw", "%",
			"minimum % of total available dx bandwidth to use\n\t\t\t\t", 
			20, 0.0, 100.0),

   multipleTargets("multitarget", "", "observe multiple targets", ChoiceOff),

   sunAvoidAngleDeg("sunavoid", "deg", "sun avoidance angle", 
			60, 0, 180),

   moonAvoidAngleDeg("moonavoid", "deg", "moon avoidance angle", 
			10, 0, 180),

   geosatAvoidAngleDeg("geosatavoid", "deg", "geostationary satellite band dec \n\t\t\t\tavoidance angle", 
			0, 0, 90),

   zenithAvoidAngleDeg("zenithavoid", "deg", "zenith avoidance angle", 
			5, 0, 90),

   autoRiseTimeCutoffMinutes("autorisecutoff", "minutes",
			     "time cutoff used to determine autorise targets\n\t\t\t\t", 
			     10, 0, 180),

   minTargetSepBeamsizes("mintargetsep", "beamsizes", "minimum target separation",
			 5, 0, 100),

   waitTargetComplete("targetwait", "", 
		      "wait for target complete before changing to \n\t\t\t\tnext target", ChoiceOn),

   decLowerLimitDeg("declowerlimit", "deg", "minimum target declination", 
			-90, -90, 90),

   decUpperLimitDeg("decupperlimit", "deg", "maximum target declination", 
		    90, -90, 90),

   repeatCount("repeatstrat", "count", "repeat strategy <count> times", 
	       1, 1, 1000000),

   maxActFailures("maxfailures", "count", 
		  "maximum number of sequential activity failures \n\t\t\t\tallowed", 
		  1, 1, 1000),

   pauseTimeBetweenActRestartsSecs(
      "restartpause", "secs", 
      "number of seconds to pause between activity \n\t\t\t\trestart attempts", 
      30, 0, 1000),

   tscopeReadyMaxFailures(
      "tscopemaxfailures", "count", 
      "maximum number of sequential 'tscope ready' \n\t\t\t\tact start failures allowed", 
      5, 1, 1000),

   tscopeReadyPauseSecs(
      "tscopereadypause", "secs", 
      "number of seconds to pause between \n\t\t\t\t'tscope ready' activity restart attempts", 
      25, 0, 1000),

   sendEmailOnStrategyFailure(
      "emailstratfail", "", "send out email notification of strategy failure\n\t\t\t\t",
      ChoiceOff),

   strategyFailureEmailAddressList(
      "emailaddr", "", "mailing addresses for strategy failure \n\t\t\t\tnotification",
      "observing@seti.org"),

   stopOnStrategyFailure(
      "stopstratfail", "", "stop on strategy failure",
      ChoiceOn),

   checkTargets("checktargets", "", "validate targets (visible, etc.)\n\t\t\t\t", ChoiceOff),

   targetAvailActSetupTime(
      "targetavailactsetup", "secs", 
      "estimated act setup time, used when determining \n\t\t\t\ttarget availability",
      60, 0, 120),

   beamBandwidthMhz("beambandwidth", "MHz", "beam input bandwidth", 
                    30, 5, 50),

   primaryTargetIdCountCutoff(
      "primaryidcutoff", "count", 
      "min number of targets with shared primary \n\t\t\t\ttarget id that must be visible", 
      120, 1, 1000),

   catalogsHighPriorityMaxCounts(
      "catshighmaxcounts", "count", 
      "high priority catalog maximum counts cutoff\n\t\t\t\t", 
      20000, 1, 50000),

   catalogsHighPriority(
      "catshigh", "comma separated catalog names", "high priority catalog names\n\t\t\t\t",
      "habcat"),

   catalogsLowPriority(
      "catslow", "comma separated catalog names", "low priority catalog names\n\t\t\t\t",
      "tycho2subset,tycho2remainder"),

   commensalCalEnabled("comcal", "", "enable periodic commensal cals", ChoiceOff),

   commensalCalIntervalMinutes("comcalinterval", "minutes",
			     "wait interval between commensal cals", 
			     60, 30, 360),

   commensalCalLengthMinutes("comcallength", "minutes",
                          "commensal cal duration", 
                          2, 1, 20),
   
   rotatePrimaryIdsEnabled("rotateids", "", "enable periodic rotation of primary target ids\n\t\t\t\t", ChoiceOff),

   rotatePrimaryIdsIntervalMinutes("rotateidsinterval", "minutes",
                                   "time interval between primary target ids \n\t\t\t\trotation", 
                                   60, 30, 360),

   tasks("tasks", "", "comma separated list of tasks to run", "obs"),

   targetMeritFactors("targetmerit", "", "comma separated list of target merit factors \n\t\t\t\tto apply", "timeleft")
{
  
   rfTune.addChoice(RfTuneOpts[RF_TUNE_AUTO]); // use automatic (non-direct) tuning
   // regardless of value set by rf
   rfTune.addChoice(RfTuneOpts[RF_TUNE_USER]); // use tuning mode set by user in rf parameters

   dxTune.addChoice(TuneDxsOpts[Param::TUNE_DXS_USER]);
   dxTune.addChoice(TuneDxsOpts[Param::TUNE_DXS_RANGE]); // observe a fixed range of frequencies
   dxTune.addChoice(TuneDxsOpts[Param::TUNE_DXS_FOREVER]);

   target.addChoice(
      ChooseTargetOpts[Param::CHOOSE_TARGET_USER]); // user: primary & synth beams 
   target.addChoice(
      ChooseTargetOpts[Param::CHOOSE_TARGET_SEMIAUTO]); // user: primary, sched: synth
   target.addChoice(
      ChooseTargetOpts[Param::CHOOSE_TARGET_AUTO]);  // sched: primary & synth
   target.addChoice(
      ChooseTargetOpts[Param::CHOOSE_TARGET_AUTO_RISE]); // sched: primary & synth
   target.addChoice(
      ChooseTargetOpts[Param::CHOOSE_TARGET_COMMENSAL]); // sched: use existing primary, sched: synth

   pipelining.addChoice(ChoiceOn);
   pipelining.addChoice(ChoiceOff);

   followup.addChoice(ChoiceOff);
   followup.addChoice(ChoiceOn);

   followupMode.addChoice(FollowupModeOpts[FOLLOWUP_MODE_AUTO]);
   followupMode.addChoice(FollowupModeOpts[FOLLOWUP_MODE_USER]);

   useBeam1.addChoice(ChoiceOn);
   useBeam1.addChoice(ChoiceOff);

   useBeam2.addChoice(ChoiceOn);
   useBeam2.addChoice(ChoiceOff);

   useBeam3.addChoice(ChoiceOn);
   useBeam3.addChoice(ChoiceOff);

   useBeam4.addChoice(ChoiceOn);
   useBeam4.addChoice(ChoiceOff);

   useBeam5.addChoice(ChoiceOn);
   useBeam5.addChoice(ChoiceOff);

   useBeam6.addChoice(ChoiceOn);
   useBeam6.addChoice(ChoiceOff);

   multipleTargets.addChoice(ChoiceOff);
   multipleTargets.addChoice(ChoiceOn);

   waitTargetComplete.addChoice(ChoiceOn);
   waitTargetComplete.addChoice(ChoiceOff);

   sendEmailOnStrategyFailure.addChoice(ChoiceOn);
   sendEmailOnStrategyFailure.addChoice(ChoiceOff);

   stopOnStrategyFailure.addChoice(ChoiceOn);
   stopOnStrategyFailure.addChoice(ChoiceOff);

   checkTargets.addChoice(ChoiceOff);  // don't check targets
   checkTargets.addChoice(ChoiceWarn); // check targets, but warn only
   checkTargets.addChoice(ChoiceOn);   // full target checking

   commensalCalEnabled.addChoice(ChoiceOn);
   commensalCalEnabled.addChoice(ChoiceOff);

   rotatePrimaryIdsEnabled.addChoice(ChoiceOn);
   rotatePrimaryIdsEnabled.addChoice(ChoiceOff);

   vector<string> meritFactors = TargetMerit::getAllMeritNames();
   for (unsigned int i=0; i<meritFactors.size(); ++i)
   {
      targetMeritFactors.addChoice(meritFactors[i]);
   }
}

SchedulerParameters::SchedulerParameters(string command) : 
   SeekerParameterGroup(command, DbTableName, IdColNameInActsTable),
   internal_(new SchedulerParametersInternal())
{
   addParameters();
}

SchedulerParameters::SchedulerParameters(const SchedulerParameters& rhs):
   SeekerParameterGroup(rhs.getCommand(), rhs.getDbTableName(),
			rhs.getIdColNameInActsTable()),
   internal_(new SchedulerParametersInternal(*rhs.internal_))
{
   setSite(rhs.getSite());
   addParameters();
}

SchedulerParameters& SchedulerParameters::operator=(const SchedulerParameters& rhs)
{
   if (this == &rhs) {
      return *this;
   }
  
   setCommand(rhs.getCommand());
   eraseParamList();
   setSite(rhs.getSite());
   delete internal_;
   internal_ = new SchedulerParametersInternal(*rhs.internal_);

   addParameters();
   return *this;
}

  
SchedulerParameters::~SchedulerParameters()
{
   delete internal_;
}


void SchedulerParameters::addParameters()
{
   addParam(internal_->rfTune);
   addParam(internal_->dxTune);
   addParam(internal_->target);
   addParam(internal_->pipelining);
   addParam(internal_->beginObsFreqMhz);
   addParam(internal_->endObsFreqMhz);
   addParam(internal_->dxRound);
   addParam(internal_->dxOverlap);
   addParam(internal_->dxTuningTolerance);
   addParam(internal_->minNumberReservedFollowupObs);
   addParam(internal_->followup);
   addParam(internal_->followupMode);
   addParam(internal_->useBeam1);
   addParam(internal_->useBeam2);
   addParam(internal_->useBeam3);
   addParam(internal_->useBeam4);
   addParam(internal_->useBeam5);
   addParam(internal_->useBeam6);
   addParam(internal_->maxTargetDistLightYears);
   addParam(internal_->minDxPercentBwToUse);
   addParam(internal_->multipleTargets);
   addParam(internal_->sunAvoidAngleDeg);
   addParam(internal_->moonAvoidAngleDeg);
   addParam(internal_->geosatAvoidAngleDeg);
   addParam(internal_->zenithAvoidAngleDeg);
   addParam(internal_->autoRiseTimeCutoffMinutes);
   addParam(internal_->minTargetSepBeamsizes);
   addParam(internal_->waitTargetComplete);
   addParam(internal_->repeatCount); 
   addParam(internal_->maxActFailures); 
   addParam(internal_->pauseTimeBetweenActRestartsSecs); 
   addParam(internal_->tscopeReadyMaxFailures); 
   addParam(internal_->tscopeReadyPauseSecs); 
   addParam(internal_->decLowerLimitDeg);
   addParam(internal_->decUpperLimitDeg);
   addParam(internal_->sendEmailOnStrategyFailure);
   addParam(internal_->strategyFailureEmailAddressList);
   addParam(internal_->stopOnStrategyFailure);
   addParam(internal_->checkTargets);
   addParam(internal_->targetAvailActSetupTime);
   addParam(internal_->beamBandwidthMhz);
   addParam(internal_->primaryTargetIdCountCutoff);
   addParam(internal_->catalogsHighPriorityMaxCounts);
   addParam(internal_->catalogsHighPriority);
   addParam(internal_->catalogsLowPriority);
   addParam(internal_->commensalCalEnabled);
   addParam(internal_->commensalCalIntervalMinutes);
   addParam(internal_->commensalCalLengthMinutes);
   addParam(internal_->rotatePrimaryIdsEnabled);
   addParam(internal_->rotatePrimaryIdsIntervalMinutes);
   addParam(internal_->tasks);
   addParam(internal_->targetMeritFactors);

   sort();
}

bool SchedulerParameters::setRfTune(string value)
{
   return internal_->rfTune.setCurrent(value);
}

bool SchedulerParameters::isAutoTuneRf() const
{
   if (internal_->rfTune.getCurrent() == RfTuneOpts[RF_TUNE_AUTO])
   {
      return true;
   }
   else if (internal_->rfTune.getCurrent() == RfTuneOpts[RF_TUNE_USER])
   {
      return false;
   }

   AssertMsg(0, "Incorrect value for isAutoTuneRf");

}

bool SchedulerParameters::setTuneDxs(string value)
{
   return internal_->dxTune.setCurrent(value);
}


SchedulerParameters::TuneDxsOption
SchedulerParameters::getTuneDxsOption() const
{
   string dxTuneString(internal_->dxTune.getCurrent());
   if (dxTuneString == TuneDxsOpts[TUNE_DXS_RANGE])
   {
      return TUNE_DXS_RANGE;
   }
   else if (dxTuneString == TuneDxsOpts[TUNE_DXS_USER])
   {
      return TUNE_DXS_USER;
   }  
   else if (dxTuneString == TuneDxsOpts[TUNE_DXS_FOREVER])
   {
      return TUNE_DXS_FOREVER;
   } 

   AssertMsg(0, "Incorrect value for tune dxs");

}


bool SchedulerParameters::setChooseTarget(string value)
{
   return internal_->target.setCurrent(value);
}

string SchedulerParameters::getChooseTargetOptionString() const
{
   return internal_->target.getCurrent();
}

SchedulerParameters::ChooseTargetOption
SchedulerParameters::getChooseTargetOption() const
{
   string chooseTargetString(internal_->target.getCurrent());
   if (chooseTargetString == ChooseTargetOpts[CHOOSE_TARGET_USER])
   {
      return CHOOSE_TARGET_USER;
   }
   else if (chooseTargetString == ChooseTargetOpts[CHOOSE_TARGET_SEMIAUTO])
   {
      return CHOOSE_TARGET_SEMIAUTO;
   }  
   else if (chooseTargetString == ChooseTargetOpts[CHOOSE_TARGET_AUTO])
   {
      return CHOOSE_TARGET_AUTO;
   }  
   else if (chooseTargetString == ChooseTargetOpts[CHOOSE_TARGET_AUTO_RISE])
   {
      return CHOOSE_TARGET_AUTO_RISE;
   } 
   else if (chooseTargetString == ChooseTargetOpts[CHOOSE_TARGET_COMMENSAL])
   {
      return CHOOSE_TARGET_COMMENSAL;
   } 

   AssertMsg(0, "Incorrect value for target option");

}


bool SchedulerParameters::pipeliningEnabled() const 
{
   if (internal_->pipelining.getCurrent() == ChoiceOn)
   {
      return true;
   }
   else if (internal_->pipelining.getCurrent() == ChoiceOff)
   {
      return false;
   }

   AssertMsg(0, "Incorrect value for pipeliningEnabled");

}
  
bool SchedulerParameters::setBeginObsFreqMhz(float64_t freqMhz)
{
   return internal_->beginObsFreqMhz.setCurrent(freqMhz);
}

float64_t SchedulerParameters::getBeginObsFreqMhz() const
{
   return internal_->beginObsFreqMhz.getCurrent();
}

bool SchedulerParameters::setEndObsFreqMhz(float64_t freqMhz)
{
   return internal_->endObsFreqMhz.setCurrent(freqMhz);
}

float64_t SchedulerParameters::getEndObsFreqMhz() const
{
   return internal_->endObsFreqMhz.getCurrent();
}

float64_t SchedulerParameters::getDxRound() const
{
   return internal_->dxRound.getCurrent();

}

int SchedulerParameters::getMinNumberReservedFollowupObs() const
{
   return internal_->minNumberReservedFollowupObs.getCurrent();
}

float64_t SchedulerParameters::getDxOverlap() const
{
   return internal_->dxOverlap.getCurrent();
}

float64_t SchedulerParameters::getDxTuningTolerance() const
{
   return internal_->dxTuningTolerance.getCurrent();
}

bool SchedulerParameters::followupEnabled() const
{
   if (internal_->followup.getCurrent() == ChoiceOn)
   {
      return true;
   }
   else if (internal_->followup.getCurrent() == ChoiceOff)
   {
      return false;
   }
 
   AssertMsg(0, "Incorrect value for followupEnabled");

}

bool SchedulerParameters::followupModeIsAuto() const
{
   if (internal_->followupMode.getCurrent() == 
       FollowupModeOpts[FOLLOWUP_MODE_AUTO])
   {
      return true;
   }
   else if (internal_->followupMode.getCurrent() ==
            FollowupModeOpts[FOLLOWUP_MODE_USER])
   {
      return false;
   }
 
   AssertMsg(0, "Incorrect value for followupMode");

}

float64_t SchedulerParameters::getMaxTargetDistLightYears() const
{
   return internal_->maxTargetDistLightYears.getCurrent();
}


float64_t SchedulerParameters::getMinDxPercentBwToUse() const
{
   return internal_->minDxPercentBwToUse.getCurrent();
}


double SchedulerParameters::getSunAvoidAngleDeg() const
{
   return internal_->sunAvoidAngleDeg.getCurrent();
}

double SchedulerParameters::getMoonAvoidAngleDeg() const
{
   return internal_->moonAvoidAngleDeg.getCurrent();
}

double SchedulerParameters::getGeosatAvoidAngleDeg() const
{
   return internal_->geosatAvoidAngleDeg.getCurrent();
}

double SchedulerParameters::getZenithAvoidAngleDeg() const
{
   return internal_->zenithAvoidAngleDeg.getCurrent();
}

double SchedulerParameters::getAutoRiseTimeCutoffMinutes() const
{
   return internal_->autoRiseTimeCutoffMinutes.getCurrent();
}

bool SchedulerParameters::getWaitTargetComplete() const
{
   if (internal_->waitTargetComplete.getCurrent() == ChoiceOn)
   {
      return true;
   }
   else if (internal_->waitTargetComplete.getCurrent() == ChoiceOff)
   {
      return false;
   }
 
   AssertMsg(0, "Incorrect value for waitTargetComplete");

}

double SchedulerParameters::getDecLowerLimitDeg() const
{
   return internal_->decLowerLimitDeg.getCurrent();
}

double SchedulerParameters::getDecUpperLimitDeg() const
{
   return internal_->decUpperLimitDeg.getCurrent();
}

int SchedulerParameters::getRepeatCount() const
{
   return internal_->repeatCount.getCurrent();
}

int SchedulerParameters::getMaxActFailures() const
{
   return internal_->maxActFailures.getCurrent();
}

int SchedulerParameters::getPrimaryTargetIdCountCutoff() const
{
   return internal_->primaryTargetIdCountCutoff.getCurrent();
}

int SchedulerParameters::getHighPriorityCatalogMaxCounts() const
{
   return internal_->catalogsHighPriorityMaxCounts.getCurrent();
}

const string SchedulerParameters::getHighPriorityCatalogNames() const
{
   return internal_->catalogsHighPriority.getCurrent();
}

const string SchedulerParameters::getLowPriorityCatalogNames() const
{
   return internal_->catalogsLowPriority.getCurrent();
}


int SchedulerParameters::getPauseTimeBetweenActRestartsSecs() const
{
   return internal_->pauseTimeBetweenActRestartsSecs.getCurrent();
}

int SchedulerParameters::getTscopeReadyMaxFailures() const
{
   return internal_->tscopeReadyMaxFailures.getCurrent();
}

int SchedulerParameters::getTscopeReadyPauseSecs() const
{
   return internal_->tscopeReadyPauseSecs.getCurrent();
}


double SchedulerParameters::getMinTargetSepBeamsizes() const
{
   return internal_->minTargetSepBeamsizes.getCurrent();
}


bool SchedulerParameters::useMultipleTargets() const
{
   if (internal_->multipleTargets.getCurrent() == ChoiceOn)
   {
      return true;
   }
   else if (internal_->multipleTargets.getCurrent() == ChoiceOff)
   {
      return false;
   }
 
   AssertMsg(0, "Incorrect value for multipleTargets");

}

// returns list of beam names that have been turned on.
vector<string> SchedulerParameters::getBeamsToUse()
{
   typedef list<ChoiceParameter<string> *> ChoiceParamList;
   ChoiceParamList useBeamParamList;
   useBeamParamList.push_back(&internal_->useBeam1);
   useBeamParamList.push_back(&internal_->useBeam2);
   useBeamParamList.push_back(&internal_->useBeam3);
   useBeamParamList.push_back(&internal_->useBeam4);
   useBeamParamList.push_back(&internal_->useBeam5);
   useBeamParamList.push_back(&internal_->useBeam6);

   vector<string> beamNames;
   for (ChoiceParamList::iterator it = useBeamParamList.begin(); 
	it != useBeamParamList.end(); ++it)
   {
      ChoiceParameter<string> *useBeamParam = *it; 
      if (useBeamParam->getCurrent() == ChoiceOn)
      {
	 beamNames.push_back(useBeamParam->getName());
      }
   }

   return beamNames;

}


bool SchedulerParameters::sendEmailOnStrategyFailure() const
{
   string opt = internal_->sendEmailOnStrategyFailure.getCurrent();
   if (opt == ChoiceOn)
   {
      return true;
   }
   else if (opt == ChoiceOff)
   {
      return false;
   }
   AssertMsg(0, "Unexpected sendEmailOnStrategyFailure option");

}

const string SchedulerParameters::getStrategyFailureEmailAddressList() const
{
   return internal_->strategyFailureEmailAddressList.getCurrent();
}

bool SchedulerParameters::stopOnStrategyFailure() const
{
   string opt = internal_->stopOnStrategyFailure.getCurrent();
   if (opt == ChoiceOn)
   {
      return true;
   }
   else if (opt == ChoiceOff)
   {
      return false;
   }

   AssertMsg(0, "Unexpected stopOnStrategyFailure option");
}


SchedulerParameters::CheckTargetsOption
SchedulerParameters::getCheckTargetsOption() const
{
   string checkString(internal_->checkTargets.getCurrent());
   if (checkString == ChoiceOn)
   {
      return CHECK_TARGETS_ON;
   }
   else if (checkString == ChoiceWarn)
   {
      return CHECK_TARGETS_WARN;
   }
   else if (checkString == ChoiceOff)
   {
      return CHECK_TARGETS_OFF;
   }

   AssertMsg(0, "Incorrect value for getCheckTargetsOption");

}

int SchedulerParameters::getTargetAvailActSetupTimeSecs() const
{
   return internal_->targetAvailActSetupTime.getCurrent();
}

double SchedulerParameters::getBeamBandwidthMhz() const
{
   return internal_->beamBandwidthMhz.getCurrent();
}

bool SchedulerParameters::commensalCalEnabled() const
{
   if (internal_->commensalCalEnabled.getCurrent() == ChoiceOn)
   {
      return true;
   }
   else if (internal_->commensalCalEnabled.getCurrent() == ChoiceOff)
   {
      return false;
   }

   AssertMsg(0, "Incorrect value for commensalCalEnabled");
}

double SchedulerParameters::getCommensalCalIntervalMinutes() const
{
   return internal_->commensalCalIntervalMinutes.getCurrent();
}

double SchedulerParameters::getCommensalCalLengthMinutes() const
{
   return internal_->commensalCalLengthMinutes.getCurrent();
}

bool SchedulerParameters::rotatePrimaryTargetIdsEnabled() const
{
   if (internal_->rotatePrimaryIdsEnabled.getCurrent() == ChoiceOn)
   {
      return true;
   }
   else if (internal_->rotatePrimaryIdsEnabled.getCurrent() == ChoiceOff)
   {
      return false;
   }

   AssertMsg(0, "Incorrect value for rotatePrimaryIdsEnabled");
}

double SchedulerParameters::getRotatePrimaryTargetIdsIntervalMinutes() const
{
   return internal_->rotatePrimaryIdsIntervalMinutes.getCurrent();
}


const string SchedulerParameters::getTasks() const
{
   return internal_->tasks.getCurrent();
}

void SchedulerParameters::addTaskType(const string & taskName)
{
   internal_->tasks.addChoice(taskName);
}

const string SchedulerParameters::getTargetMeritNames() const
{
   return internal_->targetMeritFactors.getCurrent();
}
