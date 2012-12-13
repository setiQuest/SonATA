/*******************************************************************************

 File:    ActParameters.cpp
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



#include <ace/OS.h>
#include "ActParameters.h" 
#include "Scheduler.h"
#include "SseUtil.h"
#include "Assert.h"
#include "RangeParameter.h"
#include "ChoiceParameter.h"
#include "AnyValueParameter.h"
#include "Followup.h"
#include "SseMessage.h"
#include <algorithm>
#include <sstream>

using namespace std;
typedef ActParameters Param;

static const char *DbTableName="ActParameters";
static const char *IdColNameInActsTable="actParametersId";
const int MaxTargetId = 99000000;
const int MaxBeams = 6;
const double MaxRaHours = 24.0;
const double MaxDecDeg = 90.0;
static const char *ChoiceOn = "on";
static const char *ChoiceOff = "off";
static const char *CandArchOpts[] = { "all", "confirmed", "none" };
static const char *FreqInvertOpts[] = { "rf", "if", "always", "never" };
static const char *PrimaryBeamPosTypeOpts[] = { "targetid", "coords" };
static const char *NullTypeOpts[] = { "none", "axial", "projection" };

const int TscopeTimeoutDefaultSecs = 1200;  // enough for slew + calibration

struct ActParametersInternal
{
public:

   // methods:
   ActParametersInternal();  

   // default copy constructor and assignment operator are safe

   // parameters
   RangeParameter<int> startDelaySecs;
   ChoiceParameter<string> activityType;
   RangeParameter<TargetId> targetIdBeam1;
   RangeParameter<TargetId> targetIdBeam2;
   RangeParameter<TargetId> targetIdBeam3;
   RangeParameter<TargetId> targetIdBeam4;
   RangeParameter<TargetId> targetIdBeam5;
   RangeParameter<TargetId> targetIdBeam6;
   RangeParameter<TargetId> primaryTargetId;
   RangeParameter<double> diffUtcUt1;
   AnyValueParameter<string> earthEphemFilename;  
   ChoiceParameter<string> candidateArchiveOption;
   ChoiceParameter<string> compareDxs;
   ChoiceParameter<string> emailActStatus;
   AnyValueParameter<string> emailActStatusAddressList;
   ChoiceParameter<string> siteName;
   RangeParameter<int> previousActivityId;
   ChoiceParameter<string> useWatchdogTimers;
   RangeParameter<int> sigDetWaitFactor;
   RangeParameter<int> componentReadyTimeoutSecs;
   RangeParameter<int> tscopeReadyTimeoutSecs;
   ChoiceParameter<string> freqInvert;
   ChoiceParameter<string> checkVarianceErrorLimits;
   RangeParameter<double> varianceErrorLowerLimit;
   RangeParameter<double> varianceErrorUpperLimit;
   ChoiceParameter<string> checkVarianceWarnLimits;
   RangeParameter<double> varianceWarnLowerLimit;
   RangeParameter<double> varianceWarnUpperLimit;
   RangeParameter<double> diskPercentFullWarningLimit;
   RangeParameter<double> diskPercentFullErrorLimit;
   RangeParameter<double> recentRfiAgeLimitDays;
   RangeParameter<int> dataCollCompleteTimeoutOffsetSecs;
   RangeParameter<double> doneSendingCwCohSigsTimeoutFactorPercent;
   ChoiceParameter<string> pointPrimaryBeam;
   ChoiceParameter<string> primaryBeamPositionType;
   RangeParameter<double> primaryBeamRaHours;
   RangeParameter<double> primaryBeamDecDeg;
   ChoiceParameter<string> offActNulls;
   ChoiceParameter<string> multiTargetNulls;
   RangeParameter<double> nullDepthDb;

   Scheduler* scheduler; 

};

ActParametersInternal::ActParametersInternal():
   startDelaySecs(
      "delay", "sec", "data collection delay",
      10, 0, 768),
   activityType(
      "type", "", "activity type",
      "iftest"),   // TBD: assumes this type exists
   targetIdBeam1(
      "targetbeam1", "SETI target id", "SETI target id for beam1",
      0, 0, MaxTargetId),
   targetIdBeam2(
      "targetbeam2", "SETI target id", "SETI target id for beam2",
      0, 0, MaxTargetId),
   targetIdBeam3(
      "targetbeam3", "SETI target id", "SETI target id for beam3",
      0, 0, MaxTargetId),
   targetIdBeam4(
      "targetbeam4", "SETI target id", "SETI target id for beam4",
      0, 0, MaxTargetId),
   targetIdBeam5(
      "targetbeam5", "SETI target id", "SETI target id for beam5",
      0, 0, MaxTargetId),
   targetIdBeam6(
      "targetbeam6", "SETI target id", "SETI target id for beam6",
      0, 0, MaxTargetId),
   primaryTargetId(
      "targetprimary", "target id", "target id at the primary FOV center",
      1, 1, MaxTargetId),
   diffUtcUt1(
      "dut", "sec", "UTC diff from UT1 (UT1R-UTC from IERS Bulletin B)",
      -.359341, -1, 1),
   earthEphemFilename(
      "earthephem", "", "earth ephemeris file in $SSE_SETUP directory",
      "earth.xyz"),
   candidateArchiveOption(
      "candarch", "", "candidates to archive",
      CandArchOpts[Param::ARCHIVE_CONFIRMED_CANDIDATES]),
   compareDxs(
      "comparedxs", "", "compare dx data products",
      ChoiceOff),
   emailActStatus(
      "emailactstat", "", "send out email notification of activity status",
      ChoiceOff),
   emailActStatusAddressList(
      "emailaddr", "", "mailing addresses for activity status notification",
      "observing@seti.org"),
   siteName(
      "site", "", "name of site",
      "ATA"),
   previousActivityId(
      "prevactid", "", "previous activity ID (for followup observation)", 
      0, 0, static_cast<int>(1e9)),
   useWatchdogTimers(
      "watchdogs", "", "use component watchdog timers",
      ChoiceOn),
   sigDetWaitFactor(
      "sigdetwait", "X times data coll time",
      "signal detection watchdog timer wait factor", 
      3, 1, 20),
   componentReadyTimeoutSecs(
      "readytimeout", "sec",
      "component ready watchdog timeout period", 
      60, 15, 120),
   tscopeReadyTimeoutSecs(
      "tscopetimeout", "sec",
      "tscope ready watchdog timeout period", 
      TscopeTimeoutDefaultSecs, 15, 3600),
   freqInvert(
      "freqinvert", "", "apply dx (DDC) freq inversion when using this part \n\t\t\t\tof the signal path",
      FreqInvertOpts[Param::FREQ_INVERT_NEVER]),
   checkVarianceErrorLimits(
      "varerror", "", "check stx variance error limits",
      ChoiceOn),
   varianceErrorLowerLimit(
      "varerrorlower", "",
      "stx variance error lower limit", 
      1, 0, 10000),
   varianceErrorUpperLimit(
      "varerrorupper", "",
      "stx variance error upper limit", 
      20000, 0, 100000),
   checkVarianceWarnLimits(
      "varwarn", "", "check stx variance warning limits",
      ChoiceOn),
   varianceWarnLowerLimit(
      "varwarnlower", "",
      "stx variance warn lower limit", 
      12, 0, 10000),
   varianceWarnUpperLimit(
      "varwarnupper", "",
      "stx variance warn upper limit", 
      100, 0, 100000),
   diskPercentFullWarningLimit(
      "diskfullwarn", "%",
      "archive disk full warning limit", 
      95, 0, 100),
   diskPercentFullErrorLimit(
      "diskfullerror", "%",
      "archive disk full error limit", 
      99, 0, 100),
   recentRfiAgeLimitDays(
      "rfiagelimit", "days",
      "recent RFI age limit", 
      7.0, 0.0, 28.0),
   dataCollCompleteTimeoutOffsetSecs(
      "datacolltimeoutoffset", 
      "how long to wait after expected DC complete time \n\t\t\t\tbefore timing out",
      "secs", 
      10, 1, 30),
   doneSendingCwCohSigsTimeoutFactorPercent(
      "cwcohdonetimeoutfactor", "percent of total signal detection timeout period \n\t\t\t\tto wait for 'done sending cw coherent signals' messages",
      "percent", 
      0.7, 0.1, 0.9),
   pointPrimaryBeam(
      "pointprimary", "", "point primary beam",
      ChoiceOn),
   primaryBeamPositionType(
      "primarybeampos", "", "primary beam pointing position type",
      PrimaryBeamPosTypeOpts[Param::PRIMARY_BEAM_POS_TARGET_ID]),
   primaryBeamRaHours(
      "primaryrahours", "j2000 hours",
      "primary beam pointing position RA", 
      0, 0, MaxRaHours),
   primaryBeamDecDeg(
      "primarydecdeg", "j2000 degrees",
      "primary beam pointing position Dec", 
      0, -MaxDecDeg, MaxDecDeg),

   offActNulls(
      "offactnulls", "", "specify which type of nulls should be used in OFF acts\n\t\t\t\t",
      NullTypeOpts[Param::NULL_PROJECTION]),

   multiTargetNulls(
      "multitargetnulls", "", "insert nulls for targets used on other beams",
      ChoiceOn),

   nullDepthDb(
      "nulldepth", "dB",
      "expected minimum null effectiveness", 
      7, 0, 100),

   scheduler(0)
{
}


ActParameters::ActParameters(string command) :
   SeekerParameterGroup(command, DbTableName, IdColNameInActsTable),
   internal_(new ActParametersInternal())
{ 

   // candidate archive options
   internal_->candidateArchiveOption.addChoice(
      CandArchOpts[Param::ARCHIVE_ALL_CANDIDATES]);
   internal_->candidateArchiveOption.addChoice(
      CandArchOpts[Param::ARCHIVE_CONFIRMED_CANDIDATES]);
   internal_->candidateArchiveOption.addChoice(
      CandArchOpts[Param::ARCHIVE_NO_CANDIDATES]);

   internal_->compareDxs.addChoice(ChoiceOn);
   internal_->compareDxs.addChoice(ChoiceOff);

   internal_->emailActStatus.addChoice(ChoiceOn);
   internal_->emailActStatus.addChoice(ChoiceOff);

   internal_->siteName.addChoice("ATA");

   internal_->useWatchdogTimers.addChoice(ChoiceOn);
   internal_->useWatchdogTimers.addChoice(ChoiceOff);

   internal_->freqInvert.addChoice(FreqInvertOpts[Param::FREQ_INVERT_RF]);
   internal_->freqInvert.addChoice(FreqInvertOpts[Param::FREQ_INVERT_IF]);
   internal_->freqInvert.addChoice(FreqInvertOpts[Param::FREQ_INVERT_ALWAYS]);
   internal_->freqInvert.addChoice(FreqInvertOpts[Param::FREQ_INVERT_NEVER]);

   internal_->checkVarianceErrorLimits.addChoice(ChoiceOn);
   internal_->checkVarianceErrorLimits.addChoice(ChoiceOff);

   internal_->checkVarianceWarnLimits.addChoice(ChoiceOn);
   internal_->checkVarianceWarnLimits.addChoice(ChoiceOff);

   internal_->pointPrimaryBeam.addChoice(ChoiceOn);
   internal_->pointPrimaryBeam.addChoice(ChoiceOff);

   internal_->primaryBeamPositionType.addChoice(
      PrimaryBeamPosTypeOpts[Param::PRIMARY_BEAM_POS_TARGET_ID]);
   internal_->primaryBeamPositionType.addChoice(
      PrimaryBeamPosTypeOpts[Param::PRIMARY_BEAM_POS_COORDS]);

   internal_->offActNulls.addChoice(NullTypeOpts[Param::NULL_NONE]);
   internal_->offActNulls.addChoice(NullTypeOpts[Param::NULL_AXIAL]);
   internal_->offActNulls.addChoice(NullTypeOpts[Param::NULL_PROJECTION]);

   internal_->multiTargetNulls.addChoice(ChoiceOn);
   internal_->multiTargetNulls.addChoice(ChoiceOff);

   addParameters();
   // Note: activityType choices are added by the Scheduler

   addAllImmedCmdHelp();
}

ActParameters::ActParameters(const ActParameters& rhs):
   SeekerParameterGroup(rhs.getCommand(), rhs.getDbTableName(),
			rhs.getIdColNameInActsTable()),
   internal_(new ActParametersInternal(*rhs.internal_))
{
   setSite(rhs.getSite());
   addParameters();
}

ActParameters& ActParameters::operator=(const ActParameters& rhs)
{
   if (this == &rhs) {
      return *this;
   }
  
   setCommand(rhs.getCommand());
   eraseParamList();
   setSite(rhs.getSite());
   delete internal_;
   internal_ = new ActParametersInternal(*rhs.internal_);

   addParameters();
   return *this;
}

  

ActParameters::~ActParameters()
{
   delete internal_;
}

void ActParameters::addParameters()
{
   addParam(internal_->startDelaySecs);
   addParam(internal_->activityType);
   addParam(internal_->targetIdBeam1);
   addParam(internal_->targetIdBeam2);
   addParam(internal_->targetIdBeam3);
   addParam(internal_->targetIdBeam4);
   addParam(internal_->targetIdBeam5);
   addParam(internal_->targetIdBeam6);
   addParam(internal_->primaryTargetId);
   addParam(internal_->diffUtcUt1);
   addParam(internal_->earthEphemFilename);
   addParam(internal_->candidateArchiveOption);
   addParam(internal_->compareDxs);
   addParam(internal_->emailActStatus);
   addParam(internal_->emailActStatusAddressList);
   addParam(internal_->siteName);
   addParam(internal_->previousActivityId);
   addParam(internal_->useWatchdogTimers);
   addParam(internal_->sigDetWaitFactor);
   addParam(internal_->componentReadyTimeoutSecs);
   addParam(internal_->tscopeReadyTimeoutSecs);
   addParam(internal_->freqInvert);
   addParam(internal_->checkVarianceErrorLimits);
   addParam(internal_->varianceErrorLowerLimit);
   addParam(internal_->varianceErrorUpperLimit);
   addParam(internal_->checkVarianceWarnLimits);
   addParam(internal_->varianceWarnLowerLimit);
   addParam(internal_->varianceWarnUpperLimit);
   addParam(internal_->diskPercentFullWarningLimit);
   addParam(internal_->diskPercentFullErrorLimit);
   addParam(internal_->recentRfiAgeLimitDays);
   addParam(internal_->dataCollCompleteTimeoutOffsetSecs);
   addParam(internal_->doneSendingCwCohSigsTimeoutFactorPercent);
   addParam(internal_->pointPrimaryBeam);
   addParam(internal_->primaryBeamPositionType);
   addParam(internal_->primaryBeamRaHours);
   addParam(internal_->primaryBeamDecDeg);
   addParam(internal_->offActNulls);
   addParam(internal_->multiTargetNulls);
   addParam(internal_->nullDepthDb);

   sort();
}


void ActParameters::setScheduler(Scheduler* scheduler)
{
   Assert(scheduler);
   internal_->scheduler = scheduler;
}

Scheduler* ActParameters::getScheduler()
{
   return(internal_->scheduler);
}

const char *ActParameters::status() const
{
   Assert(internal_->scheduler);
   return internal_->scheduler->getStatus();
}

// clear the list of followup activity ids
void ActParameters::clearfollowuplist() 
{
   Followup::instance()->clearActivityIdList();
}

// clear the list of followup activity ids
void ActParameters::addfollowupactid(ActivityId_t actId) 
{
   Followup::instance()->addActivityId(actId);
}


void ActParameters::addAllImmedCmdHelp()
{
   addImmedCmdHelp("status \t\t\tdisplay status for all activities ");
   addImmedCmdHelp("clearfollowuplist \tclear the list of pending followup activity Ids ");
   addImmedCmdHelp("addfollowupactid <actId> \tadd actId to the list of pending followup activity Ids ");
}


int ActParameters::getStartDelaySecs() const
{
   return internal_->startDelaySecs.getCurrent();
}

const string ActParameters::getActivityType() const
{
   return internal_->activityType.getCurrent();
}

bool ActParameters::setActivityType(const string & actTypeName)
{
   return internal_->activityType.setCurrent(actTypeName);
}


RangeParameter<TargetId> * ActParameters::
findTargetIdParamForBeamname(const string & beamName) const
{
   string methodName("findTargetIdParamForBeamname");

   // go through each beam param in turn, looking for a name match
   const string & targetbeamParamPrefix = "target";
   const string & paramNameToFind =  targetbeamParamPrefix + beamName;

   typedef list<RangeParameter<TargetId> *> RangeParamList;
   RangeParamList beamParamList;

   beamParamList.push_back(&internal_->targetIdBeam1);
   beamParamList.push_back(&internal_->targetIdBeam2);
   beamParamList.push_back(&internal_->targetIdBeam3);
   beamParamList.push_back(&internal_->targetIdBeam4);
   beamParamList.push_back(&internal_->targetIdBeam5);
   beamParamList.push_back(&internal_->targetIdBeam6);
   beamParamList.push_back(&internal_->primaryTargetId);

   for (RangeParamList::iterator it = beamParamList.begin(); 
	it != beamParamList.end(); ++it)
   {
      RangeParameter<TargetId> *beamParam = *it; 
      if (paramNameToFind == beamParam->getName())
      {
	 return beamParam;
      }
   }

   // Not found:
   stringstream strm;
   strm << "ActParameters::" << methodName
	<< ": Could not find parameter '"
	<< paramNameToFind << "' to match beam '" << beamName << "'\n";
    
   throw SseException(strm.str(), __FILE__, __LINE__,
		      SSE_MSG_INVALID_PARMS, SEVERITY_ERROR);
    
}


// Throws SseException if does not find parameter that corresponds
// to the beamName
TargetId ActParameters::getTargetIdForBeam(const string & beamName) const
{
   TargetId targetId = findTargetIdParamForBeamname(beamName)->getCurrent();
   return targetId;
}

void ActParameters::setTargetIdForBeam(const string & beamName,
				       TargetId targetId) const
{
   findTargetIdParamForBeamname(beamName)->setCurrent(targetId);
}

int ActParameters::getPrimaryTargetId() const
{
   return internal_->primaryTargetId.getCurrent();
}

int ActParameters::setPrimaryTargetId(int targetId) const
{
   return internal_->primaryTargetId.setCurrent(targetId);
}


const string ActParameters::getSiteName() const
{
   return internal_->siteName.getCurrent();
}

double ActParameters::getDiffUtcUt1() const
{
   return internal_->diffUtcUt1.getCurrent();
}

const string ActParameters::getEarthEphemFilename() const
{
   return internal_->earthEphemFilename.getCurrent();
}

ActParameters::CandidateArchiveOption
ActParameters::getCandidateArchiveOption() const
{
   string archOpt = internal_->candidateArchiveOption.getCurrent();

   if (archOpt == CandArchOpts[ARCHIVE_ALL_CANDIDATES])
   {
      return ARCHIVE_ALL_CANDIDATES;
   }
   else if (archOpt == CandArchOpts[ARCHIVE_CONFIRMED_CANDIDATES])
   {
      return ARCHIVE_CONFIRMED_CANDIDATES;
   }
   else if (archOpt == CandArchOpts[ARCHIVE_NO_CANDIDATES])
   {
      return ARCHIVE_NO_CANDIDATES;
   }
   
   AssertMsg(0, "Unexpected archive option");
}

bool ActParameters::compareDxDataProducts() const
{
   return convertOnOffToBool(internal_->compareDxs);
}

bool ActParameters::emailActStatus() const
{
   return convertOnOffToBool(internal_->emailActStatus);
}

const string ActParameters::getEmailActStatusAddressList() const
{
   return internal_->emailActStatusAddressList.getCurrent();
}

bool ActParameters::useWatchdogTimers() const
{
   return convertOnOffToBool(internal_->useWatchdogTimers);
}

int ActParameters::getSigDetWaitFactor() const
{
   return internal_->sigDetWaitFactor.getCurrent();
}

int ActParameters::getComponentReadyTimeoutSecs() const
{
   return internal_->componentReadyTimeoutSecs.getCurrent();
}

int ActParameters::getTscopeReadyTimeoutSecs() const
{
   return internal_->tscopeReadyTimeoutSecs.getCurrent();
}


void ActParameters::addActivityType(const string & actTypeName)
{
   internal_->activityType.addChoice(actTypeName);

   internal_->activityType.sort();
}

int ActParameters::getPreviousActivityId() const
{
   return internal_->previousActivityId.getCurrent();
}

void ActParameters::setPreviousActivityId(int activityId)
{
   internal_->previousActivityId.setCurrent(activityId);
}


ActParameters::FreqInvertOption 
ActParameters::getFreqInvertOption() const
{
   string option = internal_->freqInvert.getCurrent();

   if (option == FreqInvertOpts[FREQ_INVERT_RF])
   {
      return FREQ_INVERT_RF;
   }
   else if (option == FreqInvertOpts[FREQ_INVERT_IF])
   {
      return FREQ_INVERT_IF;
   }
   else if (option == FreqInvertOpts[FREQ_INVERT_ALWAYS])
   {
      return FREQ_INVERT_ALWAYS;
   }
   else if (option == FreqInvertOpts[FREQ_INVERT_NEVER])
   {
      return FREQ_INVERT_NEVER;
   }

   AssertMsg(0, "Unexpected freq invert option");
}

bool ActParameters::checkVarianceErrorLimits()
{
   return convertOnOffToBool(internal_->checkVarianceErrorLimits);
}

double ActParameters::varianceErrorLowerLimit()
{  
   return internal_->varianceErrorLowerLimit.getCurrent();
}

double ActParameters::varianceErrorUpperLimit()
{
   return internal_->varianceErrorUpperLimit.getCurrent();
}

bool ActParameters::checkVarianceWarnLimits()
{
   return convertOnOffToBool(internal_->checkVarianceWarnLimits);
}

double ActParameters::varianceWarnLowerLimit()
{
   return internal_->varianceWarnLowerLimit.getCurrent();  
}

double ActParameters::varianceWarnUpperLimit()
{
   return internal_->varianceWarnUpperLimit.getCurrent();
}

double ActParameters::getDiskPercentFullWarningLimit()
{
   return internal_->diskPercentFullWarningLimit.getCurrent();
}

double ActParameters::getDiskPercentFullErrorLimit()
{
   return internal_->diskPercentFullErrorLimit.getCurrent();
}

double ActParameters::getRecentRfiAgeLimitDays()
{
   return internal_->recentRfiAgeLimitDays.getCurrent();
}

int ActParameters::getDataCollCompleteTimeoutOffsetSecs()
{
   return internal_->dataCollCompleteTimeoutOffsetSecs.getCurrent();
}

double ActParameters::getDoneSendingCwCohSigsTimeoutFactorPercent()
{
   return internal_->doneSendingCwCohSigsTimeoutFactorPercent.getCurrent();
}
 
bool ActParameters::pointPrimaryBeam() const
{
   return convertOnOffToBool(internal_->pointPrimaryBeam);
}

void ActParameters::setPointPrimaryBeam(bool mode)
{
   if (mode)
   {
      internal_->pointPrimaryBeam.setCurrent(ChoiceOn);
   }
   else
   {
      internal_->pointPrimaryBeam.setCurrent(ChoiceOff);
   }
}


ActParameters::PrimaryBeamPositionType 
ActParameters::getPrimaryBeamPositionType() const
{
   string opt = internal_->primaryBeamPositionType.getCurrent();

   if (opt == PrimaryBeamPosTypeOpts[PRIMARY_BEAM_POS_TARGET_ID])
   {
      return PRIMARY_BEAM_POS_TARGET_ID;
   }
   else if (opt == PrimaryBeamPosTypeOpts[PRIMARY_BEAM_POS_COORDS])
   {
      return PRIMARY_BEAM_POS_COORDS;
   }

   AssertMsg(0, "Unexpected getPrimaryBeamPositionType option");
}

void ActParameters::setPrimaryBeamPositionType(PrimaryBeamPositionType posType)
{
   if (posType == PRIMARY_BEAM_POS_TARGET_ID)
   {
      internal_->primaryBeamPositionType.setCurrent(
         PrimaryBeamPosTypeOpts[PRIMARY_BEAM_POS_TARGET_ID]);
   }
   else if (posType == PRIMARY_BEAM_POS_COORDS)
   {
      internal_->primaryBeamPositionType.setCurrent(
         PrimaryBeamPosTypeOpts[PRIMARY_BEAM_POS_COORDS]);
   }
   else
   {
      AssertMsg(0, "Unexpected setPrimaryBeamPositionType option");
   }
}

double ActParameters::getPrimaryBeamRaHours() const
{
   return internal_->primaryBeamRaHours.getCurrent();
}

double ActParameters::getPrimaryBeamDecDeg() const
{
   return internal_->primaryBeamDecDeg.getCurrent();
}

void ActParameters::setPrimaryBeamRaHours(double raHours)
{
   internal_->primaryBeamRaHours.setCurrent(raHours); 
}

void ActParameters::setPrimaryBeamDecDeg(double decDeg)
{
   internal_->primaryBeamDecDeg.setCurrent(decDeg); 
};

ActParameters::NullType 
ActParameters::getOffActNullType() const
{
   string option = internal_->offActNulls.getCurrent();
   if (option == NullTypeOpts[NULL_NONE])
   {
      return NULL_NONE;
   }
   else if (option == NullTypeOpts[NULL_AXIAL])
   {
      return NULL_AXIAL;
   }
   else if (option == NullTypeOpts[NULL_PROJECTION])
   {
      return NULL_PROJECTION;
   }

   AssertMsg(0, "Unexpected offActNullType option");
}

bool ActParameters::useMultiTargetNulls() const
{
   return convertOnOffToBool(internal_->multiTargetNulls);
}

double ActParameters::getNullDepthDb() const
{
   return internal_->nullDepthDb.getCurrent();
}
