/*******************************************************************************

 File:    DxProxy.cpp
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
#include "DxProxy.h"
#include "SseDxMsg.h"
#include "SseMsg.h"
#include "ActivityUnit.h"
#include "Assert.h"
#include "DebugLog.h"
#include "ActUnitList.h"
#include "SseArchive.h"
#include "SseMessage.h"
#include "SseAstro.h"
#include "AtaInformation.h"
#include "MsgSender.h"
#include <iostream>
#include <algorithm>
#include <map>

using namespace std;

// must include this to get the specialized NssComponentManager
// template methods
#include "DxComponentManager.h"

// create a dxActivityStatus map indexed by activityId
typedef map<int, DxActivityStatus *> DxActStatMap;

const double DefaultDxSkyFreqMhz = AtaInformation::AtaDefaultSkyFreqMhz; 
const double DefaultChannelNumber = 0;

struct DxProxyInternal
{
   DxProxyInternal(DxProxy *proxy,
                    NssComponentManager<DxProxy> *siteDxManager);
   ~DxProxyInternal();

    // --- private routines to process incoming messages from Dx -----

   void sendDxMessage(NssMessage *nssMessage, int activityId);
   void sendIntrinsics(DxIntrinsics *intrinsics);  
   void sendStatus(DxStatus *status); 
   void dxTuned(ActivityUnit *actUnit, DxTuned *dxTuned);
   void baselineInitAccumStarted(ActivityUnit *actUnit);
   void baselineInitAccumComplete(ActivityUnit *actUnit);
   void dataCollectionStarted(ActivityUnit *actUnit);
   void dataCollectionComplete(ActivityUnit *actUnit);
   void signalDetectionStarted(ActivityUnit *actUnit);
   void signalDetectionComplete(ActivityUnit *actUnit);
   void sendBaseline(ActivityUnit *actUnit, void *msgBody);
   void sendComplexAmplitudes(ActivityUnit *actUnit, void *msgBody);
   void sendBaselineStatistics(ActivityUnit *actUnit, 
                               BaselineStatistics *baselineStats);
   void baselineWarningLimitsExceeded(ActivityUnit *actUnit, 
                                      BaselineLimitsExceededDetails *details);
   void baselineErrorLimitsExceeded(ActivityUnit *actUnit, 
                                    BaselineLimitsExceededDetails *details);

   void beginSendingCandidates(ActivityUnit *actUnit, Count *count);
   void sendCandidatePulseSignal(ActivityUnit *actUnit, void *msgBody);
   void sendCandidateCwPowerSignal(ActivityUnit *actUnit,
                                   CwPowerSignal *cwPowerSignal);
   void doneSendingCandidates(ActivityUnit *actUnit);

   void beginSendingSignals(ActivityUnit *actUnit, 
                            DetectionStatistics *stats);
   void sendPulseSignal(ActivityUnit *actUnit, void *msgBody);
   void sendCwPowerSignal(ActivityUnit *actUnit, CwPowerSignal *cwPowerSignal);
   void doneSendingSignals(ActivityUnit *actUnit);

   void beginSendingBadBands(ActivityUnit *actUnit, 
			     Count *count);
   void sendPulseBadBand(ActivityUnit *actUnit, PulseBadBand *pulseBadBand);
   void sendCwBadBand(ActivityUnit *actUnit, CwBadBand *cwBadBand);
   void doneSendingBadBands(ActivityUnit *actUnit);

   void beginSendingCwCoherentSignals(ActivityUnit *actUnit, Count *count);
   void sendCwCoherentSignal(ActivityUnit *actUnit,
                             CwCoherentSignal *cwCoherentSignal);
   void doneSendingCwCoherentSignals(ActivityUnit *actUnit);

    // from secondary processing
   void beginSendingCandidateResults(ActivityUnit *actUnit, const Count *count);
   void sendPulseCandidateResult(ActivityUnit *actUnit, void *msgBody);
   void sendCwCoherentCandidateResult(ActivityUnit *actUnit, 
                                      const CwCoherentSignal *cwCoherentSignal);
   void doneSendingCandidateResults(ActivityUnit *actUnit);

   void archiveComplete(ActivityUnit *actUnit);

   void dxActivityComplete(ActivityUnit *actUnit, DxActivityStatus *status);

    // --- utilities ---
   void sendFreqMaskMsg(DxMessageCode code,
                        const FrequencyMaskHeader &maskHeader,
                        const FrequencyBand freqBandArray[]);

   void sendRecentRfiMaskMsg(DxMessageCode code,
                             const RecentRfiMaskHeader &maskHeader,
                             const FrequencyBand freqBandArray[]);

   void sendPulseSignalMsg(int code, int activityId, const PulseSignalHeader &hdr, 
                           Pulse pulses[]);

   int getVerboseLevel();
   void logBadMessageSize(SseInterfaceHeader *hdr);

   bool lookupActivityUnit(int activityId, ActivityUnit **actUnit);

   void createDxActivityStatus(ActivityUnit *actUnit);
   void destroyDxActivityStatus(ActivityUnit *actUnit);
   void setDxActivityStatus(ActivityUnit *actUnit, DxActivityState state);
   void setDxActivityStatus(int activityId, DxActivityState state);
   void updateDxStatus();
   string getNameInternal();
   void setNameInternal(const string &name);

   // --- data ---
   DxProxy *proxy_;
   NssComponentManager<DxProxy> *siteDxManager_;
   DxIntrinsics intrinsics_;
   DxConfiguration config_;
   DxStatus status_;
   string dxName_;
   ActUnitList actUnitList_;
   DxActStatMap dxActStatMap_;  // activity status map indexed by actUnit
   float64_t dxSkyFreqMhz_;
   int32_t channelNumber_;

   ACE_Recursive_Thread_Mutex objectMutex_;
   ACE_Recursive_Thread_Mutex nameMutex_;
   ACE_Recursive_Thread_Mutex skyFreqMutex_;
   ACE_Recursive_Thread_Mutex chanNumberMutex_;
   ACE_Recursive_Thread_Mutex statusMutex_;
   ACE_Recursive_Thread_Mutex intrinsicsMutex_;

};

DxProxyInternal::DxProxyInternal(DxProxy *proxy, 
				   NssComponentManager<DxProxy> *siteDxManager)
    : proxy_(proxy),
      siteDxManager_(siteDxManager),
      dxName_("dx-unknown"),
      dxSkyFreqMhz_(DefaultDxSkyFreqMhz),
      channelNumber_(DefaultChannelNumber)
{
    // defaults
    SseUtil::strMaxCpy(intrinsics_.interfaceVersionNumber,
		       SSE_DX_INTERFACE_VERSION, MAX_TEXT_STRING);
    
    // should this be stored?
    status_.timestamp = SseMsg::currentNssDate();
    status_.numberOfActivities = 0;

}

DxProxyInternal::~DxProxyInternal()
{

}

string DxProxyInternal::getNameInternal()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(nameMutex_);

    return dxName_;
}

void DxProxyInternal::setNameInternal(const string &name)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(nameMutex_);

    dxName_ = name;
}




// ---- private routines to handle incoming messages from physical dx ---
//------------------------------------------------------------------------

void DxProxyInternal::createDxActivityStatus(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(statusMutex_);

    Assert(actUnit);
    DxActStatMap &actStatMap = dxActStatMap_;

    int activityId = actUnit->getActivityId();    

    // actUnit shouldn't already haved an associated status
    Assert(actStatMap.find(activityId) == actStatMap.end());

    // insert a new DxActivityStatus into the map, indexed by activityId
    DxActivityStatus *actStatus = new DxActivityStatus;
    actStatus->activityId = activityId;
    actStatus->currentState = DX_ACT_NONE;

    actStatMap[activityId] = actStatus;
}

void DxProxyInternal::destroyDxActivityStatus(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(statusMutex_);

    Assert(actUnit);
    DxActStatMap &actStatMap = dxActStatMap_;

    // Make sure it's there
    int activityId = actUnit->getActivityId();
    Assert(actStatMap.find(activityId) != actStatMap.end());

    // clean up  DxActivityStatus
    DxActivityStatus *actStatus = actStatMap[activityId];
    actStatMap.erase(activityId);
    delete actStatus;
}

void DxProxyInternal::setDxActivityStatus(ActivityUnit *actUnit,
					    DxActivityState state)
{
    // no mutex needed

    Assert(actUnit);
    int activityId = actUnit->getActivityId();

    setDxActivityStatus(activityId, state);
}

void DxProxyInternal::setDxActivityStatus(int activityId,
					    DxActivityState state)
{
    {
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(statusMutex_);

	// Verify that the activityId is in the map.
	// If actUnit has detached, then it won't be there;
	// send back a warning.
	if (dxActStatMap_.find(activityId) == dxActStatMap_.end())
	{
	    stringstream strm;
	    strm << "DxProxy::setDxActivityStatus()  "
		 << getNameInternal()
		 << " could not set status for activity Id " << activityId
		 << endl;
	    SseMessage::log(getNameInternal(),
                            activityId, SSE_MSG_INFO,
                            SEVERITY_WARNING, strm.str(),
                            __FILE__, __LINE__);
	    return;
	}
	
	DxActivityStatus *actStatus = dxActStatMap_[activityId];
	actStatus->currentState = state;
	
    }

    // update the overall dx status
    updateDxStatus();

}

void DxProxyInternal::updateDxStatus()
{
    { 
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(statusMutex_);

	// update the overall dx status by
	// copying in the status for each pipelined activity

	status_.timestamp = SseMsg::currentNssDate();
	status_.numberOfActivities = 0;

	const unsigned int maxDxActivities = MAX_DX_ACTIVITIES;
	if (dxActStatMap_.size() > maxDxActivities)
	{
	    stringstream strm;
	    strm << "DxProxy::updateDxStatus()  "
				   << getNameInternal()
				   << " has more than than the maximum number"
				   << " (" <<  maxDxActivities << ")"
				   << " of activities connected. " << endl;
	    SseMessage::log(getNameInternal(),
                            NSS_NO_ACTIVITY_ID, SSE_MSG_2MANY_ACT,
                            SEVERITY_ERROR, strm.str(),
                            __FILE__, __LINE__);
	}

	// should be no more that MAX_DX_ACTIVITIES running
	//AssertMsg (dxActStatMap_.size() <= MAX_DX_ACTIVITIES, "Exceeded max number of activities");

	DxActStatMap::iterator p;
	unsigned int count;
	for (p = dxActStatMap_.begin(), count=0;
	     p != dxActStatMap_.end() && count < maxDxActivities;
	     ++p, ++count)
	{
	    DxActivityStatus *dxActStat = p->second;
	    if (dxActStat->currentState != DX_ACT_NONE)
	    {
		status_.act[status_.numberOfActivities] = *dxActStat;
		status_.numberOfActivities++;
	    }
	}
    }  // end ACE_Guard on statusMutex_


    // don't mutex protect this call so that 
    // status can be read back without blocking

    siteDxManager_->notifyStatusChanged(proxy_);

}


void DxProxyInternal::sendDxMessage(NssMessage *nssMessage, int activityId)
{

    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward error to the site.
    siteDxManager_->processNssMessage(proxy_, *nssMessage, activityId,
				       proxy_->getRemoteHostname());

    // forward the error to the activityUnit(s)

    // If activity Id matches one of the activity units,
    // then only send the message to that unit.
    // If activityId is NSS_NO_ACTIVITY_ID (or zero) then 
    // send to all attached activityUnits.

    // work with a local copy of the act unit list since
    // it may be modified as a result of disconnection

    ActUnitList localActUnitList = actUnitList_;
    ActUnitList::iterator p;
    for (p = localActUnitList.begin(); p != localActUnitList.end(); ++p)
    {
	ActivityUnit *actUnit = *p;
	Assert(actUnit);  // make sure it's not null
	if (activityId == NSS_NO_ACTIVITY_ID  
	    || activityId == 0  
	    || actUnit->getActivityId() == activityId)
	{
	    actUnit->sendDxMessage(proxy_, *nssMessage);
	}
    }

}

void DxProxyInternal::sendIntrinsics(DxIntrinsics *intrinsics)
{
   {
      ACE_Guard<ACE_Recursive_Thread_Mutex> guard(intrinsicsMutex_);
      
      intrinsics_ = *intrinsics;

      // use the name as the dx name (dx identifier)
      setNameInternal(intrinsics_.name);

      //TBD. Do something with intrinsics_.serialNumber;

      VERBOSE2(getVerboseLevel(), intrinsics_);

   }

   siteDxManager_->receiveIntrinsics(proxy_);

}

void DxProxyInternal::sendStatus(DxStatus *stat)
{
    {
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(statusMutex_);

	status_ = *stat;
    }

    updateDxStatus();

}

void DxProxyInternal::dxTuned(ActivityUnit *actUnit, DxTuned *dxTuned)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    setDxActivityStatus(actUnit, DX_ACT_TUNED);

    // forward message to activityUnit
    actUnit->dxTuned(proxy_, *dxTuned);

}



void DxProxyInternal::baselineInitAccumStarted(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(getVerboseLevel(),
	      getNameInternal() << ": baselineInitAccumStarted" << endl;);

    // set activity status
    setDxActivityStatus(actUnit, DX_ACT_RUN_BASE_ACCUM);

    // forward message to activityUnit
    // TBD

}

void DxProxyInternal::baselineInitAccumComplete(ActivityUnit *actUnit)

{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(getVerboseLevel(),
	     getNameInternal() << ":baselineInitAccumComplete" << endl;);

    // set activity status
    setDxActivityStatus(actUnit, DX_ACT_BASE_ACCUM_COMPLETE);

    setDxActivityStatus(actUnit, DX_ACT_PEND_DC);

    // forward message to activityUnit
    // TBD
}



void DxProxyInternal::dataCollectionStarted(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(getVerboseLevel(),
	      getNameInternal() << ": Data Collection started" << endl;);

    // set activity status
    setDxActivityStatus(actUnit, DX_ACT_RUN_DC);

    // forward message to activityUnit
    actUnit->dataCollectionStarted(proxy_);

}



void DxProxyInternal::dataCollectionComplete(ActivityUnit *actUnit)

{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(getVerboseLevel(),
	     getNameInternal() << ": Data Collection complete" << endl;);

    // set activity status
    setDxActivityStatus(actUnit, DX_ACT_DC_COMPLETE);

    // Assume that a signal detection is pending.
    // This is slightly misleading for a DC only activity,
    // but in that case the status should update very quickly
    // to the proper value.
    setDxActivityStatus(actUnit, DX_ACT_PEND_SD);

    // forward message to activityUnit
    actUnit->dataCollectionComplete(proxy_);
}

void DxProxyInternal::signalDetectionStarted(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(getVerboseLevel(),
	      getNameInternal() << ": Signal Detection started" << endl;);

    // set activity status
    setDxActivityStatus(actUnit, DX_ACT_RUN_SD);

    // forward message to activityUnit
    actUnit->signalDetectionStarted(proxy_);

}

void DxProxyInternal::signalDetectionComplete(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(getVerboseLevel(),
	     getNameInternal() << ": Signal Detection complete" << endl;);

    // set activity status
    setDxActivityStatus(actUnit, DX_ACT_SD_COMPLETE);

    // forward message to activityUnit
    actUnit->signalDetectionComplete(proxy_);

}

void DxProxyInternal::sendBaseline(ActivityUnit *actUnit, void *msgBody)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    BaselineHeader *hdr;
    BaselineValue *valueArray;
    SseDxMsg::extractBaselineFromMsg(msgBody, &hdr, &valueArray);

    // send Baseline info to the activityUnit
    actUnit->sendBaseline(proxy_, *hdr, valueArray);
}

void DxProxyInternal::sendComplexAmplitudes(ActivityUnit *actUnit, void *msgBody)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    ComplexAmplitudeHeader *hdr;
    SubchannelCoef1KHz *subchannelArray;
    SseDxMsg::extractComplexAmplitudesFromMsg(msgBody, &hdr, &subchannelArray);

    // send info to the activityUnit
    actUnit->sendComplexAmplitudes(proxy_, *hdr, subchannelArray);
}


void DxProxyInternal::sendBaselineStatistics(ActivityUnit *actUnit, 
					      BaselineStatistics *baselineStats)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->sendBaselineStatistics(proxy_, *baselineStats);

}

void DxProxyInternal::baselineWarningLimitsExceeded(ActivityUnit *actUnit, 
				BaselineLimitsExceededDetails *details)
{ 
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    actUnit->baselineWarningLimitsExceeded(proxy_, *details);

}

void DxProxyInternal::baselineErrorLimitsExceeded(ActivityUnit *actUnit, 
				BaselineLimitsExceededDetails *details)
{ 
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    actUnit->baselineErrorLimitsExceeded(proxy_, *details);

}


void DxProxyInternal::beginSendingCandidates(ActivityUnit *actUnit, Count *count)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->beginSendingCandidates(proxy_, *count);
}

void DxProxyInternal::sendCandidatePulseSignal(ActivityUnit *actUnit, void *msgBody)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    PulseSignalHeader *hdr;
    Pulse *pulseArray;
    SseDxMsg::extractPulseSignalFromMsg(msgBody, &hdr, &pulseArray);

    // forward message to activityUnit
    actUnit->sendCandidatePulseSignal(proxy_, *hdr, pulseArray);
}

void DxProxyInternal::sendCandidateCwPowerSignal(ActivityUnit *actUnit,
						  CwPowerSignal *cwPowerSignal)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->sendCandidateCwPowerSignal(proxy_, *cwPowerSignal);
}

void DxProxyInternal::doneSendingCandidates(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->doneSendingCandidates(proxy_);
}

void DxProxyInternal::beginSendingSignals(ActivityUnit *actUnit, 
					   DetectionStatistics *stats)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->beginSendingSignals(proxy_, *stats);
}

void DxProxyInternal::sendPulseSignal(ActivityUnit *actUnit, void *msgBody)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    PulseSignalHeader *hdr;
    Pulse *pulseArray;
    SseDxMsg::extractPulseSignalFromMsg(msgBody, &hdr, &pulseArray);

    // forward message to activityUnit
    actUnit->sendPulseSignal(proxy_, *hdr, pulseArray);
}

void DxProxyInternal::sendCwPowerSignal(ActivityUnit *actUnit, 
					 CwPowerSignal *cwPowerSignal)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->sendCwPowerSignal(proxy_, *cwPowerSignal);
}

void DxProxyInternal::doneSendingSignals(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->doneSendingSignals(proxy_);
}



void DxProxyInternal::beginSendingBadBands(ActivityUnit *actUnit, Count *count)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE2(getVerboseLevel(), "bad band count is: " << *count << endl;);

    // forward message to activityUnit
    actUnit->beginSendingBadBands(proxy_, *count);
    
}

void DxProxyInternal::sendCwBadBand(ActivityUnit *actUnit, 
					CwBadBand *cwBadBand)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE2(getVerboseLevel(), "cw band band is: " << *cwBadBand << endl;);

    // forward message to activityUnit
    actUnit->sendCwBadBand(proxy_, *cwBadBand);
}

void DxProxyInternal::sendPulseBadBand(ActivityUnit *actUnit, 
					PulseBadBand *pulseBadBand)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE2(getVerboseLevel(), "pulse band band is: " << *pulseBadBand << endl;);

    // forward message to activityUnit
    actUnit->sendPulseBadBand(proxy_, *pulseBadBand);

}


void DxProxyInternal::doneSendingBadBands(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->doneSendingBadBands(proxy_);
}




void DxProxyInternal::beginSendingCwCoherentSignals(ActivityUnit *actUnit,
						     Count *count)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->beginSendingCwCoherentSignals(proxy_, *count);
}

void DxProxyInternal::sendCwCoherentSignal(ActivityUnit *actUnit, 
					    CwCoherentSignal *cwCoherentSignal)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->sendCwCoherentSignal(proxy_, *cwCoherentSignal);
}

void DxProxyInternal::doneSendingCwCoherentSignals(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);    

    // forward message to activityUnit
    actUnit->doneSendingCwCoherentSignals(proxy_);
}


// --- private routines to handle relayed messages from secondary processing ------
//------------------------------------------------------------------------
void DxProxyInternal::beginSendingCandidateResults(ActivityUnit *actUnit,
						    const Count *count)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->beginSendingCandidateResults(proxy_, *count);
};

void DxProxyInternal::sendPulseCandidateResult(ActivityUnit *actUnit, void *msgBody)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    PulseSignalHeader *hdr;
    Pulse *pulseArray;
    SseDxMsg::extractPulseSignalFromMsg(msgBody, &hdr, &pulseArray);

    // forward message to activityUnit
    actUnit->sendPulseCandidateResult(proxy_, *hdr, pulseArray);
};

void DxProxyInternal::sendCwCoherentCandidateResult(
    ActivityUnit *actUnit,
    const CwCoherentSignal *cwCoherentSignal)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->sendCwCoherentCandidateResult(proxy_, *cwCoherentSignal);
};

void DxProxyInternal::doneSendingCandidateResults(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->doneSendingCandidateResults(proxy_);
};


// -- handle archive & wrapup ------



void DxProxyInternal::archiveComplete(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // forward message to activityUnit
    actUnit->archiveComplete(proxy_);
}

void  DxProxyInternal::dxActivityComplete(ActivityUnit *actUnit,
					    DxActivityStatus *actstat)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE2(getVerboseLevel(), 
	     getNameInternal()  << ": dxActivityComplete: " << *actstat  << endl);

    // TBD confirm that actUnit activityId matches the
    // actstat activityId.

    // set activity status
    setDxActivityStatus(actUnit, DX_ACT_NONE);

    actUnit->dxActivityComplete(proxy_, *actstat);
    
}

// utilities

// Send a variable length FrequencyMask message
//
void DxProxyInternal::sendFreqMaskMsg(DxMessageCode code,
			       const FrequencyMaskHeader &hdr,
			       const FrequencyBand freqBandArray[])
{
    // no mutex needed

    int dataLength;
    char *msgBody = SseDxMsg::encodeFrequencyMaskIntoMsg(hdr, freqBandArray,
							  &dataLength);

    proxy_->sendMsgNoId(code, dataLength, msgBody);

    delete[] msgBody;

}

// Send a variable length RecentRfiMask message
//
void DxProxyInternal::sendRecentRfiMaskMsg(DxMessageCode code,
			       const RecentRfiMaskHeader &hdr,
			       const FrequencyBand freqBandArray[])
{
    // No mutex needed

    int dataLength;
    char *msgBody = SseDxMsg::encodeRecentRfiMaskIntoMsg(hdr, freqBandArray,
							  &dataLength);

    proxy_->sendMsgNoId(code, dataLength, msgBody);

    delete[] msgBody;

}

void DxProxyInternal::sendPulseSignalMsg(int code,
					  int activityId,
					  const PulseSignalHeader &hdr,
					  Pulse pulses[])
{
    // No mutex needed

    int dataLength;
    char *msgBody = SseDxMsg::encodePulseSignalIntoMsg(hdr, pulses, &dataLength);

    proxy_->sendMessage(code, activityId, dataLength, msgBody);

    delete[] msgBody;

}

int DxProxyInternal::getVerboseLevel()
{
    // no mutex needed

    return proxy_->getVerboseLevel();
}

void DxProxyInternal::logBadMessageSize(SseInterfaceHeader *hdr)
{
    // No mutex needed

    stringstream strm;
    strm << "DxProxy Error:" 
	   << " dataLength of " << hdr->dataLength
	   << " is invalid for message '"
	   << SseDxMsg::messageCodeToString(hdr->code)
	   << "' from dx: " << proxy_->getRemoteHostname() << endl;
    SseMessage::log(proxy_->getRemoteHostname(),
                    NSS_NO_ACTIVITY_ID, SSE_MSG_INVALID_MSG,
                    SEVERITY_ERROR, strm.str(),
                    __FILE__, __LINE__);
}


// Look up the activityUnit that corresponds to the
// given activityId, which is returned in the actUnit
// argument.  Function return value is true on success, false
// if the actUnit can't be found.

bool DxProxyInternal::lookupActivityUnit(int activityId, ActivityUnit **actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    bool found = false;

    ActUnitList::iterator p;
    for (p = actUnitList_.begin(); p != actUnitList_.end(); ++p)
    {
	ActivityUnit *localActUnit = *p;
	Assert(localActUnit);  // make sure it's not null
	if (localActUnit->getActivityId() == activityId)
	{
	    *actUnit = localActUnit;
	    found = true;
	    break;
	}
    }

    return found;

}


//  ------- end DxProxyInternal ---------
//  --------------------------------------
//  ------- begin DxProxy ---------------


DxProxy::DxProxy(NssComponentManager<DxProxy> *siteDxManager)
    : internal_(new DxProxyInternal(this, siteDxManager))
{
}

DxProxy::~DxProxy()
{
    delete internal_;

    VERBOSE2(getVerboseLevel(), 
	      "DxProxy destructor" << endl;);
}



// --------- DxProxy public utility methods ------------
// ------------------------------------------------------

void DxProxy::notifyInputConnected()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

    // Register this dx with the site.
    // Need to do this here instead of in the DxProxy constructor
    // because we don't want to register the proxy until its
    // socket connection with the real dx is established.

    internal_->siteDxManager_->registerProxy(this);

}

void DxProxy::notifyInputDisconnected()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

    // notify any attached Activity units that the socket is gone

    // work with a local copy of the act unit list since
    // it may be modified as a result of disconnection - tbd remove this?

    ActUnitList localActUnitList = internal_->actUnitList_;
    ActUnitList::iterator p;

    for (p = localActUnitList.begin(); p != localActUnitList.end(); ++p)
    {
	ActivityUnit *actUnit = *p;
	Assert(actUnit);  // make sure it's not null
	actUnit->notifyDxProxyDisconnected(this);
    }

    // unregister with the site
    internal_->siteDxManager_->unregisterProxy(this);

}


void DxProxy::attachActivityUnit(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

    // TBD error handling


    ActUnitList &actUnitList = internal_->actUnitList_;
    Assert(actUnit);   // make sure it's not null

    // may want to limit the number of actunits allowed at a time
    // just warn for now (see below)

    // TBD make sure it's not already in there
    if (find(actUnitList.begin(), actUnitList.end(), actUnit) !=
	actUnitList.end())
    {
	stringstream strm;
	strm <<
	    "DxProxy error: trying to attach activityUnit twice" << endl;
	
	SseMessage::log(MsgSender,
                        NSS_NO_ACTIVITY_ID, SSE_MSG_ALREADY_ATTACH,
                        SEVERITY_ERROR, strm.str(),
                        __FILE__, __LINE__);

	return;
    }

    actUnitList.push_back(actUnit);


    const unsigned int maxDxActivities = MAX_DX_ACTIVITIES;
    if (actUnitList.size() > maxDxActivities)
    {
	stringstream strm;
	strm << "DxProxy::attachActivityUnit Error: "
			       << getName()
			       << " has more than than the maximum number"
			       << " (" <<  maxDxActivities << ")"
			       << " of activities connected. " << endl;
	    SseMessage::log(getName(),
                            NSS_NO_ACTIVITY_ID, SSE_MSG_2MANY_ACT,
                            SEVERITY_ERROR, strm.str(),
                            __FILE__, __LINE__);
    }


    VERBOSE2(getVerboseLevel(), getName() << ": attached actUnit " 
	     << actUnit << " nUnits attached: "
	     << actUnitList.size() << endl);

    // create an associated DxActivityStatus
    internal_->createDxActivityStatus(actUnit);

}

void DxProxy::detachActivityUnit(ActivityUnit *actUnit)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

    // TBD error handling -- make this an assert?  or an exception?

    Assert (actUnit);
    ActUnitList &actUnitList = internal_->actUnitList_;

    // make sure act unit is in the list
    if (find(actUnitList.begin(), actUnitList.end(), actUnit) ==
	actUnitList.end())
    {
	stringstream strm;
	strm << "DxProxy error: trying to detach unattached activityUnit\n";

	SseMessage::log(getName(),
                        NSS_NO_ACTIVITY_ID, SSE_MSG_ERROR_DETACH,
                        SEVERITY_ERROR, strm.str(),
                        __FILE__, __LINE__);
	return;
    }

    actUnitList.remove(actUnit);

    VERBOSE2(getVerboseLevel(), getName() << ": detached actUnit " << actUnit <<
	     " nUnits attached: " << actUnitList.size() << endl);

    internal_->destroyDxActivityStatus(actUnit);
}

DxIntrinsics DxProxy::getIntrinsics()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->intrinsicsMutex_);

    return internal_->intrinsics_;
}

DxConfiguration DxProxy::getConfiguration()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

    return internal_->config_;
}

// Return the currently stored (cached) status.
// Does NOT query the dx for the latest status.
DxStatus DxProxy::getCachedDxStatus()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->statusMutex_);

    return internal_->status_;
}

// a unique identifier for this dx
string DxProxy::getName()
{
    return internal_->getNameInternal();
}

void DxProxy::setName(const string &name)
{
    internal_->setNameInternal(name);
}

string DxProxy::expectedInterfaceVersion() 
{
    return SSE_DX_INTERFACE_VERSION;
}

string DxProxy::receivedInterfaceVersion()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->intrinsicsMutex_);

   return internal_->intrinsics_.interfaceVersionNumber;
}

int DxProxy::getNumber()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->intrinsicsMutex_);

   return internal_->intrinsics_.serialNumber;
}



void DxProxy::setDxSkyFreq(float64_t skyfreq)
{
    {
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->skyFreqMutex_);

	internal_->dxSkyFreqMhz_ = skyfreq;
    }

    internal_->siteDxManager_->notifyStatusChanged(this);
}

float64_t DxProxy::getDxSkyFreq()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->skyFreqMutex_);

    return internal_->dxSkyFreqMhz_;
}

void DxProxy::setChannelNumber(int32_t chanNumber)
{
    {
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->chanNumberMutex_);

	internal_->channelNumber_ = chanNumber;
    }

    internal_->siteDxManager_->notifyStatusChanged(this);
}

int32_t DxProxy::getChannelNumber()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->chanNumberMutex_);

    return internal_->channelNumber_;
}


float64_t DxProxy::getBandwidthInMHz() const
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->intrinsicsMutex_);

    return (internal_->intrinsics_.maxSubchannels * 
	internal_->intrinsics_.hzPerSubchannel) / SseAstro::HzPerMhz;
}

void DxProxy::setIntrinsicsBandwidth(int32_t maxSubchannels,
				      float64_t hzPerSubchannel)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->intrinsicsMutex_);

    internal_->intrinsics_.maxSubchannels = maxSubchannels;
    internal_->intrinsics_.hzPerSubchannel = hzPerSubchannel;
}
    


// define a macro to validate the dataLength of an incoming message
#define ValidateMsgLength(hdr, validLengthTest) \
      if (!(validLengthTest)) \
      { \
	  internal_->logBadMessageSize(hdr); \
	  return; \
      }

// dispatch incoming dx message to appropriate
// sse classes & methods
void DxProxy::handleIncomingMessage(SseInterfaceHeader *hdr, void *bodybuff)
{

    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

//    cout << "DxProxy(handleDxMessage from " << getName() << "): " << endl;

  VERBOSE2(getVerboseLevel(),
	   "Recv Msg from " << getName() <<  ": "
	   << SseDxMsg::messageCodeToString(hdr->code) << "\n" << *hdr);


  // Determine the activityUnit that should get this message, 
  // based on the activityId in the message header.
  // Some messages may go to multiple activityUnits, 
  // or not go to any activityUnit.

  ActivityUnit *actUnit = 0;

  // Don't try to select an activityUnit for those 
  // messsage codes that don't require one.

  if (hdr->code == SEND_INTRINSICS ||
      hdr->code == SEND_DX_STATUS ||
      hdr->code == SEND_DX_MESSAGE)
  {
      // no act unit required
  }
  else
  {
      // look up activityUnit based on activityId.
      // error if not found.

      if (! internal_->lookupActivityUnit(hdr->activityId, &actUnit))
      {

// This happens regularly when activities
// are stopped, so there's not much use in logging it.
#if 0
	  SseArchive::ErrorLog() << "DxProxy Error: " << getName()
	  << " can't forward "
	  << SseDxMsg::getMessageCodeString(hdr->code)
	  << " message from DX.\n"
	  << "Activity Id " << hdr->activityId
	  << " does not match any Activity Unit." << endl;
#endif
	  
	  return;
      }
  }


  DxIntrinsics *intrinsics;
  DxStatus *status;
  DxActivityStatus *actstat;
  DxTuned *dxTuned;
  Count *count;
  CwPowerSignal *cwPowerSignal;
  CwCoherentSignal *cwCoherentSignal;
  NssMessage *nssMessage;
  BaselineStatistics *baselineStats;
  BaselineLimitsExceededDetails *baselineLimitsExceeded;
  DetectionStatistics *detStats;
  CwBadBand *cwBadBand;
  PulseBadBand *pulseBadBand;

  // dispatch here
  // process message body, if there is one
  switch (hdr->code)
  {
  case SEND_INTRINSICS:

      // store intrinsics in this proxy
      VERBOSE2(getVerboseLevel(), getName() 
	       << ": DxProxy:store intrinsics  ... " << endl;);
 
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(DxIntrinsics));
      intrinsics = static_cast<DxIntrinsics *>(bodybuff);
      intrinsics->demarshall();
      internal_->sendIntrinsics(intrinsics);

      break;

  case SEND_DX_STATUS:
 
      // store status in this proxy
      VERBOSE2(getVerboseLevel(), getName()
	       << ": store status  ... " << endl;);

      ValidateMsgLength(hdr, hdr->dataLength == sizeof(DxStatus));
      status = static_cast<DxStatus *>(bodybuff);
      status->demarshall();
      internal_->sendStatus(status);

      VERBOSE2(getVerboseLevel(), *status << endl);

      break;

  case SEND_DX_MESSAGE:

      ValidateMsgLength(hdr, hdr->dataLength == sizeof(NssMessage));
      nssMessage = static_cast<NssMessage *>(bodybuff);
      nssMessage->demarshall();
      internal_->sendDxMessage(nssMessage, hdr->activityId);
      break;

  case DX_TUNED:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(DxTuned));
      dxTuned = static_cast<DxTuned *>(bodybuff);
      dxTuned->demarshall();
      internal_->dxTuned(actUnit, dxTuned);
      break;

  case BASELINE_INIT_ACCUM_STARTED:
      internal_->baselineInitAccumStarted(actUnit);
      break;

  case BASELINE_INIT_ACCUM_COMPLETE:
      internal_->baselineInitAccumComplete(actUnit);
      break;

  case DATA_COLLECTION_STARTED:
      internal_->dataCollectionStarted(actUnit);
      break;

  case DATA_COLLECTION_COMPLETE:
      internal_->dataCollectionComplete(actUnit);
      break;

  case SIGNAL_DETECTION_STARTED:
      internal_->signalDetectionStarted(actUnit);
      break;

  case SIGNAL_DETECTION_COMPLETE:
      internal_->signalDetectionComplete(actUnit);
      break;

  case DX_ACTIVITY_COMPLETE:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(DxActivityStatus));
      actstat = static_cast<DxActivityStatus *>(bodybuff);
      actstat->demarshall();
      internal_->dxActivityComplete(actUnit, actstat);
      break;

  case SEND_BASELINE:
      ValidateMsgLength(hdr, hdr->dataLength > 0);
      internal_->sendBaseline(actUnit, bodybuff);
      break;

  case SEND_COMPLEX_AMPLITUDES:
      ValidateMsgLength(hdr, hdr->dataLength > 0);
      internal_->sendComplexAmplitudes(actUnit, bodybuff);
      break;

  case SEND_BASELINE_STATISTICS:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(BaselineStatistics));
      baselineStats = static_cast<BaselineStatistics *>(bodybuff);
      baselineStats->demarshall();
      internal_->sendBaselineStatistics(actUnit, baselineStats);
      break;

  case BASELINE_WARNING_LIMITS_EXCEEDED:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(BaselineLimitsExceededDetails));
      baselineLimitsExceeded = static_cast<BaselineLimitsExceededDetails *>(bodybuff);
      baselineLimitsExceeded->demarshall();
      internal_->baselineWarningLimitsExceeded(actUnit, baselineLimitsExceeded);
      break;

  case BASELINE_ERROR_LIMITS_EXCEEDED:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(BaselineLimitsExceededDetails));
      baselineLimitsExceeded = static_cast<BaselineLimitsExceededDetails *>(bodybuff);
      baselineLimitsExceeded->demarshall();
      internal_->baselineErrorLimitsExceeded(actUnit, baselineLimitsExceeded);
      break;

  case BEGIN_SENDING_CANDIDATES:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(Count));
      count = static_cast<Count *>(bodybuff);
      count->demarshall();
      internal_->beginSendingCandidates(actUnit, count);
      break;

  case SEND_CANDIDATE_CW_POWER_SIGNAL:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(CwPowerSignal));
      cwPowerSignal = static_cast<CwPowerSignal *>(bodybuff);
      cwPowerSignal->demarshall();
      internal_->sendCandidateCwPowerSignal(actUnit, cwPowerSignal);
      break;

  case SEND_CANDIDATE_PULSE_SIGNAL:
      ValidateMsgLength(hdr, hdr->dataLength > 0);
      internal_->sendCandidatePulseSignal(actUnit, bodybuff);
      break;

  case DONE_SENDING_CANDIDATES:
      internal_->doneSendingCandidates(actUnit);
      break;

  case BEGIN_SENDING_SIGNALS:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(DetectionStatistics));
      detStats = static_cast<DetectionStatistics *>(bodybuff);
      detStats->demarshall();
      internal_->beginSendingSignals(actUnit, detStats);
      break;

  case SEND_CW_POWER_SIGNAL:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(CwPowerSignal));
      cwPowerSignal = static_cast<CwPowerSignal *>(bodybuff);
      cwPowerSignal->demarshall();
      internal_->sendCwPowerSignal(actUnit, cwPowerSignal);
      break;

  case SEND_PULSE_SIGNAL:
      ValidateMsgLength(hdr, hdr->dataLength > 0);
      internal_->sendPulseSignal(actUnit, bodybuff);
      break;

  case DONE_SENDING_SIGNALS:
      internal_->doneSendingSignals(actUnit);
      break;


  case BEGIN_SENDING_BAD_BANDS:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(Count));
      count = static_cast<Count *>(bodybuff);
      count->demarshall();
      internal_->beginSendingBadBands(actUnit, count);
      break;

  case SEND_CW_BAD_BAND:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(CwBadBand));
      cwBadBand = static_cast<CwBadBand *>(bodybuff);
      cwBadBand->demarshall();
      internal_->sendCwBadBand(actUnit, cwBadBand);
      break;

  case SEND_PULSE_BAD_BAND:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(PulseBadBand));
      pulseBadBand = static_cast<PulseBadBand *>(bodybuff);
      pulseBadBand->demarshall();
      internal_->sendPulseBadBand(actUnit, pulseBadBand);
      break;

  case DONE_SENDING_BAD_BANDS:
      internal_->doneSendingBadBands(actUnit);
      break;

  case BEGIN_SENDING_CW_COHERENT_SIGNALS:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(Count));
      count = static_cast<Count *>(bodybuff);
      count->demarshall();
      internal_->beginSendingCwCoherentSignals(actUnit, count);
      break;

  case SEND_CW_COHERENT_SIGNAL:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(CwCoherentSignal));
      cwCoherentSignal = static_cast<CwCoherentSignal *>(bodybuff);
      cwCoherentSignal->demarshall();
      internal_->sendCwCoherentSignal(actUnit, cwCoherentSignal);
      break;

  case DONE_SENDING_CW_COHERENT_SIGNALS:
      internal_->doneSendingCwCoherentSignals(actUnit);
      break;

  case BEGIN_SENDING_CANDIDATE_RESULTS:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(Count));
      count = static_cast<Count *>(bodybuff);
      count->demarshall();
      internal_->beginSendingCandidateResults(actUnit, count);
      break;

  case SEND_PULSE_CANDIDATE_RESULT:
      ValidateMsgLength(hdr, hdr->dataLength > 0);
      internal_->sendPulseCandidateResult(actUnit, bodybuff);
      break;

  case SEND_CW_COHERENT_CANDIDATE_RESULT:
      ValidateMsgLength(hdr, hdr->dataLength == sizeof(CwCoherentSignal));
      cwCoherentSignal = static_cast<CwCoherentSignal *>(bodybuff);
      cwCoherentSignal->demarshall();
      internal_->sendCwCoherentCandidateResult(actUnit, cwCoherentSignal);
      break;

  case DONE_SENDING_CANDIDATE_RESULTS:
      internal_->doneSendingCandidateResults(actUnit);
      break;

  case ARCHIVE_COMPLETE:
      internal_->archiveComplete(actUnit);
      break;

  default: 
      stringstream strm;
      strm << "DxProxy::handleDxMessage: " 
			     <<  "unexpected message code received:"
			     << hdr->code 
			     << " from dx " << getName() << endl;
      SseMessage::log(getName(),
                      NSS_NO_ACTIVITY_ID, SSE_MSG_INVALID_MSG,
                      SEVERITY_ERROR, strm.str().c_str());
      break;
  };  


}


// *** outgoing messages to physical dx *****
//--------------------------------------------

void DxProxy::requestIntrinsics()
{
    VERBOSE2(getVerboseLevel(), 
	      getName() <<": DxProxy: sending requestIntrinsics msg to dx\n";);

    DxMessageCode code = REQUEST_INTRINSICS;
    sendMsgNoId(code);
}


void DxProxy::configureDx(const DxConfiguration &config)
{
    VERBOSE2(getVerboseLevel(), 
	      getName() << ": DxProxy: sending configureDx msg to dx\n";);

    DxMessageCode code = CONFIGURE_DX;
    int dataLength = sizeof(config);

    // marshall a local copy
    DxConfiguration marshall = config;
    marshall.marshall();
    sendMsgNoId(code, dataLength, &marshall);

    // store config info locally
    // does dxproxy really need to save this?
    {
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);
	internal_->config_=config;
    }

    VERBOSE2(getVerboseLevel(), getName() << config);
}

void DxProxy::sendPermRfiMask(const FrequencyMaskHeader &maskHeader,
			       const FrequencyBand freqBandArray[])
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending perm rfi mask msg to dx\n";);

    DxMessageCode code = PERM_RFI_MASK;

    internal_->sendFreqMaskMsg(code, maskHeader, freqBandArray);
}


void DxProxy::sendRcvrBirdieMask(const FrequencyMaskHeader &maskHeader,
			       const FrequencyBand freqBandArray[])
{
    VERBOSE2(getVerboseLevel(),     
	     getName() << ": DxProxy: sending rcvr birdie mask msg to dx\n";);

    DxMessageCode code = RCVR_BIRDIE_MASK;

    internal_->sendFreqMaskMsg(code, maskHeader, freqBandArray);

}


void DxProxy::sendBirdieMask(const FrequencyMaskHeader &maskHeader,
			       const FrequencyBand freqBandArray[])
{
    VERBOSE2(getVerboseLevel(),     
	     getName() << ": DxProxy: sending birdie mask msg to dx\n";);

    DxMessageCode code = BIRDIE_MASK;

    internal_->sendFreqMaskMsg(code, maskHeader, freqBandArray);

}

void DxProxy::sendTestSignalMask(const FrequencyMaskHeader &maskHeader,
			       const FrequencyBand freqBandArray[])
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending test signal mask msg to dx\n";);

    DxMessageCode code = TEST_SIGNAL_MASK;

    internal_->sendFreqMaskMsg(code, maskHeader, freqBandArray);

}

void DxProxy::sendRecentRfiMask(const RecentRfiMaskHeader &maskHeader,
			       const FrequencyBand freqBandArray[])
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending recent rfi mask msg to dx\n";);

    DxMessageCode code = RECENT_RFI_MASK;

    internal_->sendRecentRfiMaskMsg(code, maskHeader, freqBandArray);

}



void DxProxy::requestStatusUpdate()
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending requestDxStatus msg to dx\n";);

    DxMessageCode code = REQUEST_DX_STATUS;
    sendMsgNoId(code);

}

void DxProxy::sendDxActivityParameters(const DxActivityParameters &actParam)
{

    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending init dx (dx act params) msg to dx\n";);

    {
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);
	internal_->setDxActivityStatus(actParam.activityId, DX_ACT_INIT);
    }

    DxMessageCode code = SEND_DX_ACTIVITY_PARAMETERS;
    int dataLength = sizeof(actParam);

    DxActivityParameters marshall = actParam;
    marshall.marshall();
    sendMessage(code, actParam.activityId, dataLength, &marshall); 

    VERBOSE2(getVerboseLevel(), actParam);

}



void DxProxy::dxScienceDataRequest(const DxScienceDataRequest &dataRequest)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending data request msg to dx" << endl;);

    DxMessageCode code = DX_SCIENCE_DATA_REQUEST;
    int dataLength = sizeof(dataRequest);

    DxScienceDataRequest marshall = dataRequest;
    marshall.marshall();
    sendMsgNoId(code, dataLength, &marshall);

}


void DxProxy::setStartTime(int activityId, const StartActivity &startAct)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending start time (start act) msg to dx" << endl;);

    {
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);
	internal_->setDxActivityStatus(activityId, DX_ACT_PEND_BASE_ACCUM);
    }

    DxMessageCode code = START_TIME;
    int dataLength = sizeof(startAct);

    StartActivity marshall = startAct;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall);

}

void DxProxy::stopDxActivity(int activityId)
{
    // No mutex needed

    VERBOSE2(getVerboseLevel(),     
	     "DxProxy: sending stopDxActivity msg to dx "
	     << getName() << " for actId " << activityId << "\n");

    DxMessageCode code = STOP_DX_ACTIVITY;
    sendMessage(code, activityId);

    // Temporary code, to try to update the dx status.
    // The stop handling should really be changed so that
    // we properly process the dxActivityComplete message
    // that is sent in response to a stop.

    requestStatusUpdate();

}

void DxProxy::shutdown()
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending shutdown msg to dx\n";);

    DxMessageCode code = SHUTDOWN_DX;
    sendMsgNoId(code);
}

void DxProxy::restart()
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending restart msg to dx\n";);

    DxMessageCode code = RESTART_DX;
    sendMsgNoId(code);

}


void DxProxy::requestArchiveData(int activityId, 
				  const ArchiveRequest &archiveRequest)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sending requestArchiveData msg to dx"
	     << " for signal "  << archiveRequest.signalId.number << endl;);

    DxMessageCode code = REQUEST_ARCHIVE_DATA;
    int dataLength = sizeof(archiveRequest);

    ArchiveRequest marshall = archiveRequest;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall);

}

void DxProxy::discardArchiveData(int activityId, 
				  const ArchiveRequest &archiveRequest)
{
    VERBOSE2(getVerboseLevel(),     
	     getName() << ":DxProxy: sending discardArchiveData msg to dx"
	     << " for signal " << archiveRequest.signalId.number << endl;);

    DxMessageCode code = DISCARD_ARCHIVE_DATA;
    int dataLength = sizeof(archiveRequest);

    ArchiveRequest marshall = archiveRequest;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall);

}

// outpoing messages to the Zxs
void DxProxy::beginSendingRequestedCompampSubchannels(int activityId, 
		 const Count &nSubchannels)
{
    VERBOSE2(getVerboseLevel(), getName() << 
	    ": DxProxy: beginSendingRequestedCompampSubchannels to dx" << endl;);

    DxMessageCode code = BEGIN_SENDING_REQUESTED_COMPAMP_SUBCHANNELS;
    int dataLength = sizeof(nSubchannels);

    Count marshall = nSubchannels;
    marshall.marshall();


    //sendMessage(code, activityId, dataLength, &marshall);

}
void DxProxy::sendRequestedCompampSubchannel(int activityId, 
		const int subchannelNumber)
{
    VERBOSE2(getVerboseLevel(),     getName() << 
		    ": DxProxy: sendRequestedCompampSubchannel msg to dx" << endl;);

    DxMessageCode code = SEND_REQUESTED_COMPAMP_SUBCHANNEL;
    int dataLength = sizeof(subchannelNumber);

	unsigned int marshall = htonl(subchannelNumber);
    sendMessage(code, activityId, dataLength, &marshall);
}
void DxProxy::doneSendingRequestedCompampSubchannels(int activityId)
{
    VERBOSE2(getVerboseLevel(),     getName() << 
		    ": DxProxy: doneSendingRequestedCompampSubchannels" << endl;);

    DxMessageCode code = DONE_SENDING_REQUESTED_COMPAMP_SUBCHANNELS;
    sendMessage(code, activityId);
}
// outgoing messages to dx
// ------------------------------------
void DxProxy::beginSendingFollowUpSignals(int activityId, const Count &nSignals)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: beginSendingFollowUpSignals to dx" << endl;);

    DxMessageCode code = BEGIN_SENDING_FOLLOW_UP_SIGNALS;
    int dataLength = sizeof(nSignals);

    Count marshall = nSignals;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall);

}

void DxProxy::sendFollowUpCwSignal(int activityId,
				    const FollowUpCwSignal &followUpCwSignal)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sendFollowUpCwSignal msg to dx" << endl;);

    DxMessageCode code = SEND_FOLLOW_UP_CW_SIGNAL;
    int dataLength = sizeof(followUpCwSignal);

    FollowUpCwSignal marshall = followUpCwSignal;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall); 
}

void DxProxy::sendFollowUpPulseSignal(
    int activityId,
    const FollowUpPulseSignal &followUpPulseSignal)
{

    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sendFollowUpPulseSignal msg to dx" << endl;);

    DxMessageCode code = SEND_FOLLOW_UP_PULSE_SIGNAL;
    int dataLength = sizeof(followUpPulseSignal);

    FollowUpPulseSignal marshall = followUpPulseSignal;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall); 


}

void DxProxy::doneSendingFollowUpSignals(int activityId)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: doneSendingFollowUpCandidates" << endl;);

    DxMessageCode code = DONE_SENDING_FOLLOW_UP_SIGNALS;
    sendMessage(code, activityId);
}



// outgoing messages for seconary processing
// ------------------------------------

void DxProxy::beginSendingCandidatesSecondary (
    int activityId, const Count &count)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sendingCandidates Secondary msg to dx" << endl;);

    DxMessageCode code = BEGIN_SENDING_CANDIDATES;
    int dataLength = sizeof(count);

    Count marshall = count;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall);

}

void DxProxy::sendCandidateCwPowerSignalSecondary(
    int activityId, 
    const CwPowerSignal &cwPowerSignal)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sendCandidateCwPowerSignal Secondary msg to dx" << endl;);

    DxMessageCode code = SEND_CANDIDATE_CW_POWER_SIGNAL;
    int dataLength = sizeof(cwPowerSignal);

    CwPowerSignal marshall = cwPowerSignal;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall);

}

void DxProxy::sendCandidatePulseSignalSecondary(
    int activityId, 
    const PulseSignalHeader &hdr,
    Pulse pulses[])
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sendCandidatePulseSignal Secondary msg to dx" << endl;);

    DxMessageCode code = SEND_CANDIDATE_PULSE_SIGNAL;
    internal_->sendPulseSignalMsg(code, activityId, hdr, pulses);

}

void DxProxy::doneSendingCandidatesSecondary(int activityId)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: doneSendingCandidates Secondary msg to dx" << endl;);

    DxMessageCode code = DONE_SENDING_CANDIDATES;
    sendMessage(code, activityId);

}

void DxProxy::beginSendingCwCoherentSignalsSecondary (int activityId, 
							 const Count &count)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: beginSendingCwCoherentSignals Secondary msg to dx" << endl;);

    DxMessageCode code = BEGIN_SENDING_CW_COHERENT_SIGNALS;
    int dataLength = sizeof(count);

    Count marshall = count;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall);

}

void DxProxy::sendCwCoherentSignalSecondary(int activityId, 
					       const CwCoherentSignal &cwCoherentSignal)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: sendCwCoherentSignal Secondary msg to dx" << endl;);

    DxMessageCode code = SEND_CW_COHERENT_SIGNAL;
    int dataLength = sizeof(cwCoherentSignal);

    CwCoherentSignal marshall = cwCoherentSignal;
    marshall.marshall();
    sendMessage(code, activityId, dataLength, &marshall); 

}


void DxProxy::doneSendingCwCoherentSignalsSecondary(int activityId)
{
    VERBOSE2(getVerboseLevel(),     
	      getName() << ": DxProxy: doneSendingCwCoherentSignals Secondary msg to dx" << endl;);

    DxMessageCode code = DONE_SENDING_CW_COHERENT_SIGNALS;
    sendMessage(code, activityId);

}

// *** dxProxy private utility routines ****
//---------------------------------------------

void DxProxy::sendMsgNoId(int messageCode, int dataLength, const void *msgBody)
{
    int activityId = NSS_NO_ACTIVITY_ID;
    sendMessage(messageCode, activityId, dataLength, msgBody);
}

// send a marshalled message to the DX
void DxProxy::sendMessage(int messageCode, int activityId, int dataLength,
			   const void *msgBody)
{
    VERBOSE2(getVerboseLevel(), 
	     "Send Msg to " << getName() <<  ": "
	     << SseDxMsg::messageCodeToString(messageCode) << "\n");

    // Assume this is mutex protected by NssProxy
    NssProxy::sendMessage(messageCode, activityId, dataLength, msgBody);

}
