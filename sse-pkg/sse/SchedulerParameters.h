/*******************************************************************************

 File:    SchedulerParameters.h
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
 * PURPOSE:  
 *****************************************************************/

#ifndef SCHEDULERPARAMETERS_H
#define SCHEDULERPARAMETERS_H

#include "SeekerParameterGroup.h"
#include "Activity.h"
#include "TargetId.h"
#include "machine-dependent.h" // for float64_t et. al.
#include <vector>

class SchedulerParametersInternal;

class SchedulerParameters : public SeekerParameterGroup
{
 public:

   enum CheckTargetsOption { CHECK_TARGETS_ON, CHECK_TARGETS_WARN,
			     CHECK_TARGETS_OFF };

   enum TuneDxsOption { TUNE_DXS_RANGE, TUNE_DXS_USER,
			 TUNE_DXS_FOREVER };

   enum ChooseTargetOption { CHOOSE_TARGET_USER, CHOOSE_TARGET_SEMIAUTO,
			     CHOOSE_TARGET_AUTO, CHOOSE_TARGET_AUTO_RISE,
                             CHOOSE_TARGET_COMMENSAL };

   SchedulerParameters(string command);
   SchedulerParameters(const SchedulerParameters& rhs);
   SchedulerParameters& operator=(const SchedulerParameters& rhs);
   virtual ~SchedulerParameters();
   
   const string getTasks() const;
   bool pipeliningEnabled() const;
   bool setRfTune(string value);
   bool isAutoTuneRf() const;
   bool setTuneDxs(string value);
   TuneDxsOption getTuneDxsOption() const;

   bool setBeginObsFreqMhz(float64_t freqMhz);
   float64_t getBeginObsFreqMhz() const;

   bool setEndObsFreqMhz(float64_t freqMhz);
   float64_t getEndObsFreqMhz() const;

   ChooseTargetOption getChooseTargetOption() const;
   string getChooseTargetOptionString() const;
   bool setChooseTarget(string value);

   float64_t getDxRound() const;
   float64_t getDxOverlap() const;
   float64_t getDxTuningTolerance() const;
   int getMinNumberReservedFollowupObs() const;
   bool followupEnabled() const;
   bool followupModeIsAuto() const;
   std::vector<string> getBeamsToUse();

   double getMaxTargetDistLightYears() const;
   double getMinDxPercentBwToUse() const;
   bool useMultipleTargets() const;
   double getSunAvoidAngleDeg() const;
   double getMoonAvoidAngleDeg() const;
   double getGeosatAvoidAngleDeg() const;
   double getZenithAvoidAngleDeg() const;
   double getAutoRiseTimeCutoffMinutes() const;
   double getMinTargetSepBeamsizes() const;
   bool getWaitTargetComplete() const;
   int getRepeatCount() const;
   double getDecLowerLimitDeg() const;
   double getDecUpperLimitDeg() const;

   int getPrimaryTargetIdCountCutoff() const;
   int getHighPriorityCatalogMaxCounts() const;
   const string getHighPriorityCatalogNames() const;
   const string getLowPriorityCatalogNames() const;
   const string getTargetMeritNames() const;

   int getMaxActFailures() const;
   int getPauseTimeBetweenActRestartsSecs() const;

   int getTscopeReadyMaxFailures() const;
   int getTscopeReadyPauseSecs() const;

   bool sendEmailOnStrategyFailure() const;
   const string getStrategyFailureEmailAddressList() const;

   bool stopOnStrategyFailure() const;

   CheckTargetsOption getCheckTargetsOption() const;
   int getTargetAvailActSetupTimeSecs() const;

   double getBeamBandwidthMhz() const;

   // commensal calibrations
   bool commensalCalEnabled() const;
   double getCommensalCalIntervalMinutes() const;
   double getCommensalCalLengthMinutes() const;

   bool rotatePrimaryTargetIdsEnabled() const;
   double getRotatePrimaryTargetIdsIntervalMinutes() const;

   // utility methods
   void addTaskType(const string & taskName);

   // ----- immediate commands -----
   // none

 protected:
   void addParameters();

 private:
   SchedulerParametersInternal *internal_;
  
};

#endif /* SCHEDULERPARAMETERS_H */
