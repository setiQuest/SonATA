/*******************************************************************************

 File:    ActivityUnit.h
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

// Activity Unit Abstract Base Class

#ifndef _activityunit_h
#define _activityunit_h

// An Activity Unit coordinates the communications
// between an activity and one or a pair of DXs.

// This is a protocol class that defines *only* the
// interface to an ActivityUnit (no implementation).
// Subclasses should inherit from the ActivityUnitImp 
// implementation class.

#include "sseDxInterface.h"
#include "TargetId.h"
#include <string>
#include <vector>
#include <mysql.h>

using std::string;
using std::vector;

class DxProxy;
class DbParameters;

class ActivityUnit
{
 public:
  virtual ~ActivityUnit();

  //--- utilities
  virtual int getId() = 0;
  virtual int getActivityId() = 0;
  virtual string getDxName() = 0;
  
  //---  methods called by activity ------
  virtual void initialize() = 0;
  virtual void dxScienceDataRequest(const DxScienceDataRequest &dataRequest) = 0;
  virtual void setStartTime(const StartActivity &startAct) = 0; 
  virtual void setStartBaseAccumTime(const StartActivity &startAct) = 0; 
  virtual void setStartDataCollTime(const StartActivity &startAct) = 0; 

  virtual void sendRecentRfiMask(MYSQL *dbConn, const vector<TargetId> & targetsToExclude, struct tm *refHHMMSS) = 0;
  virtual void prepareFollowUpCandidateSignals(MYSQL *dbConn, int previousActId) = 0;
  virtual int sendFollowUpCandidateSignals(const NssDate &newStartTime) = 0;
  virtual void sendCandidatesForSecondaryProcessing(MYSQL *dbConn) = 0;
  virtual void resolveCandidatesBasedOnSecondaryProcessingResults(MYSQL *dbConn) = 0;

  virtual void stop(DbParameters &dbParam) = 0;
  virtual void shutdown() = 0;
  virtual void resetSocket() = 0;
   virtual void zxLookUpSetiLiveCandidates(MYSQL *callerDbConn) = 0;

  
  // -- methods called by dxProxy --------
  virtual void dxTuned(DxProxy* dx, const DxTuned &dxTuned) = 0;
  virtual void dataCollectionStarted (DxProxy* dx) = 0;
  virtual void dataCollectionComplete (DxProxy* dx) = 0;
  virtual void signalDetectionStarted (DxProxy* dx) = 0;
  virtual void signalDetectionComplete (DxProxy* dx) = 0;

  virtual void sendBaseline(DxProxy* dx,
			    const BaselineHeader &hdr,
			    BaselineValue valueArray[]) = 0;

  virtual void sendComplexAmplitudes(DxProxy* dx, 
				     const ComplexAmplitudeHeader &hdr,
				     SubchannelCoef1KHz subchannelArray[]) = 0;
  virtual void sendBaselineStatistics(DxProxy *dx,
				      const BaselineStatistics &baselineStats) = 0;

  virtual void baselineWarningLimitsExceeded(DxProxy *dx,
					     const BaselineLimitsExceededDetails &details) = 0;

  virtual void baselineErrorLimitsExceeded(DxProxy *dx,
					     const BaselineLimitsExceededDetails &details) = 0;


  virtual void beginSendingCandidates(DxProxy* dx, const Count &count) = 0;
  virtual void sendCandidatePulseSignal(DxProxy* dx, 
					const PulseSignalHeader &hdr,
					Pulse pulses[]) = 0;
  virtual void sendCandidateCwPowerSignal(DxProxy* dx,
					  const CwPowerSignal &cwPowerSignal) = 0;

  virtual void doneSendingCandidates(DxProxy* dx) = 0;
  
  virtual void beginSendingSignals(DxProxy* dx, const DetectionStatistics &stats) = 0;
  virtual void sendPulseSignal(DxProxy* dx,
			       const PulseSignalHeader &hdr,
			       Pulse pulses[]) = 0;
  virtual void sendCwPowerSignal(DxProxy* dx,
				 const CwPowerSignal &cwPowerSignal) = 0;
  virtual void doneSendingSignals(DxProxy* dx) = 0;

  virtual void beginSendingCwCoherentSignals(DxProxy* dx, const Count &count) = 0;
  virtual void sendCwCoherentSignal(DxProxy* dx,
				 const CwCoherentSignal &cwCoherentSignal) = 0;
  virtual void doneSendingCwCoherentSignals(DxProxy* dx) = 0;

  virtual void sendDxMessage(DxProxy *dx, const NssMessage & nssMessage) = 0;


  virtual void beginSendingBadBands(DxProxy *dx, const Count &count) = 0;
  virtual void sendCwBadBand(DxProxy *dx, const CwBadBand &cwBadBand) = 0;
  virtual void sendPulseBadBand(DxProxy *dx, const PulseBadBand &pulseBadBand) = 0;
  virtual void doneSendingBadBands(DxProxy *dx) = 0;
  
  // secondary processing results:
  virtual void beginSendingCandidateResults(DxProxy* dx, const Count &count) = 0;
  virtual void sendPulseCandidateResult(DxProxy* dx, 
					const PulseSignalHeader &hdr,
					Pulse pulses[]) = 0;
  virtual void sendCwCoherentCandidateResult(DxProxy* dx, const CwCoherentSignal &cwCoherentSignal) = 0;
  virtual void doneSendingCandidateResults(DxProxy* dx) = 0;

  virtual void archiveComplete(DxProxy *dx) = 0;

  virtual void notifyDxProxyDisconnected(DxProxy *dxProxy) = 0;


  // Warning ActivityUnit may be destroyed in this method.
  // Do not use ActivityUnit past this point.
  virtual void dxActivityComplete(DxProxy *dx, const DxActivityStatus &status) = 0;

};

#endif // _activityunit_h
