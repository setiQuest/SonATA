/*******************************************************************************

 File:    checkSseDxMsgSizes.cpp
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


// checks all the struct sizes & field offsets
// in the sseDxInterface messages, as a
// marshalling aid.

#include "sseDxInterface.h"

#include <iostream>
#include <cstddef>

using std::cout;
using std::endl;

void dumpStructInfo();

int main()
{
  dumpStructInfo();
}

#define SIZEOF(x) cout << "\nsizeof " << #x << ": " << sizeof(x) << endl

#define OFFSETOF(x,field) cout << "offsetof " << #field << ": " << offsetof(x,field) << endl

void dumpStructInfo()
{
    SIZEOF(SseInterfaceHeader);
    OFFSETOF(SseInterfaceHeader, code);
    OFFSETOF(SseInterfaceHeader, dataLength);
    OFFSETOF(SseInterfaceHeader, messageNumber);
    OFFSETOF(SseInterfaceHeader, activityId);
    OFFSETOF(SseInterfaceHeader, timestamp);
    OFFSETOF(SseInterfaceHeader, sender);
    OFFSETOF(SseInterfaceHeader, receiver);

    SIZEOF(NssMessage);
    OFFSETOF(NssMessage,code);
    OFFSETOF(NssMessage,severity);
    OFFSETOF(NssMessage,description);

    SIZEOF(HereIAm);
    OFFSETOF(HereIAm,interfaceVersionNumber);

    SIZEOF(ThereYouAre);
    OFFSETOF(ThereYouAre,sseIp);
    OFFSETOF(ThereYouAre,portId);
    OFFSETOF(ThereYouAre,interfaceVersionNumber);

    SIZEOF(DxIntrinsics);
    OFFSETOF(DxIntrinsics,interfaceVersionNumber);
    OFFSETOF(DxIntrinsics,name);
    OFFSETOF(DxIntrinsics,host);
    OFFSETOF(DxIntrinsics,codeVersion);
    OFFSETOF(DxIntrinsics,channelBase);
    OFFSETOF(DxIntrinsics,foldings);
    OFFSETOF(DxIntrinsics,oversampling);
    OFFSETOF(DxIntrinsics,filterName);
    OFFSETOF(DxIntrinsics,hzPerSubchannel);
    OFFSETOF(DxIntrinsics,maxSubchannels);
    OFFSETOF(DxIntrinsics,serialNumber);
    OFFSETOF(DxIntrinsics,birdieMaskDate);
    OFFSETOF(DxIntrinsics,permMaskDate);

    SIZEOF(DxConfiguration);
    OFFSETOF(DxConfiguration, site);
    OFFSETOF(DxConfiguration, dxId);
    OFFSETOF(DxConfiguration, a2dClockrate);
    OFFSETOF(DxConfiguration, archiverHostname);
    OFFSETOF(DxConfiguration, archiverPort);

    SIZEOF(FrequencyBand);
    OFFSETOF(FrequencyBand, centerFreq);
    OFFSETOF(FrequencyBand, bandwidth);

    SIZEOF(FrequencyMaskHeader);
    OFFSETOF(FrequencyMaskHeader, numberOfFreqBands);
    OFFSETOF(FrequencyMaskHeader, maskVersionDate);
    OFFSETOF(FrequencyMaskHeader, bandCovered);

    SIZEOF(RecentRfiMaskHeader);
    OFFSETOF(RecentRfiMaskHeader, numberOfFreqBands);
    OFFSETOF(RecentRfiMaskHeader, excludedTargetId);
    OFFSETOF(RecentRfiMaskHeader, bandCovered);

    SIZEOF(PulseParameters);
    OFFSETOF(PulseParameters, pulseThreshold);
    OFFSETOF(PulseParameters, tripletThreshold);
    OFFSETOF(PulseParameters, singletThreshold);

    SIZEOF(DxScienceData);
    OFFSETOF(DxScienceData, requestType);
    OFFSETOF(DxScienceData, subchannel);
    OFFSETOF(DxScienceData, rfFreq);

    SIZEOF(DxScienceDataRequest);
    OFFSETOF(DxScienceDataRequest, sendBaselines);
    OFFSETOF(DxScienceDataRequest, sendBaselineStatistics);
    OFFSETOF(DxScienceDataRequest, checkBaselineWarningLimits);
    OFFSETOF(DxScienceDataRequest, checkBaselineErrorLimits);
    OFFSETOF(DxScienceDataRequest, baselineReportingHalfFrames);
    OFFSETOF(DxScienceDataRequest, sendComplexAmplitudes);
    OFFSETOF(DxScienceDataRequest, scienceData);

    SIZEOF(BaselineLimits);
    OFFSETOF(BaselineLimits, meanUpperBound);
    OFFSETOF(BaselineLimits, meanLowerBound);
    OFFSETOF(BaselineLimits, stdDevPercent);
    OFFSETOF(BaselineLimits, maxRange);

    SIZEOF(DxActivityParameters);
    OFFSETOF(DxActivityParameters, activityId);
    OFFSETOF(DxActivityParameters, dataCollectionLength);
    OFFSETOF(DxActivityParameters, rcvrSkyFreq);
    OFFSETOF(DxActivityParameters, ifcSkyFreq);
    OFFSETOF(DxActivityParameters, dxSkyFreq);
    OFFSETOF(DxActivityParameters, channelNumber);
    OFFSETOF(DxActivityParameters, operations);
    OFFSETOF(DxActivityParameters, sensitivityRatio);
    OFFSETOF(DxActivityParameters, maxNumberOfCandidates);
    OFFSETOF(DxActivityParameters, clusteringFreqTolerance);
    OFFSETOF(DxActivityParameters, zeroDriftTolerance);
    OFFSETOF(DxActivityParameters, maxDriftRateTolerance);

    OFFSETOF(DxActivityParameters, badBandCwPathLimit);
    OFFSETOF(DxActivityParameters, cwClusteringDeltaFreq);
    OFFSETOF(DxActivityParameters, daddResolution);
    OFFSETOF(DxActivityParameters, daddThreshold);
    OFFSETOF(DxActivityParameters, cwCoherentThreshold);
    OFFSETOF(DxActivityParameters, secondaryCwCoherentThreshold);
    OFFSETOF(DxActivityParameters, secondaryPfaMargin);
    OFFSETOF(DxActivityParameters, limitsForCoherentDetection);

    OFFSETOF(DxActivityParameters, badBandPulseTripletLimit);
    OFFSETOF(DxActivityParameters, badBandPulseLimit);
    OFFSETOF(DxActivityParameters, pulseClusteringDeltaFreq);
    OFFSETOF(DxActivityParameters, pulseTrainSignifThresh);
    OFFSETOF(DxActivityParameters, secondaryPulseTrainSignifThresh);

    OFFSETOF(DxActivityParameters, maxPulsesPerHalfFrame);
    OFFSETOF(DxActivityParameters, maxPulsesPerSubchannelPerHalfFrame);
    OFFSETOF(DxActivityParameters, requestPulseResolution);
    OFFSETOF(DxActivityParameters, pd);

    OFFSETOF(DxActivityParameters, scienceDataRequest);
    OFFSETOF(DxActivityParameters, baselineSubchannelAverage);
    OFFSETOF(DxActivityParameters, baselineInitAccumHalfFrames);

    OFFSETOF(DxActivityParameters, baselineDecay);
    OFFSETOF(DxActivityParameters, baselineWarningLimits);
    OFFSETOF(DxActivityParameters, baselineErrorLimits);


    SIZEOF(DxTuned);
    OFFSETOF(DxTuned, dxSkyFreq);
    OFFSETOF(DxTuned, dataCollectionLength);
    OFFSETOF(DxTuned, dataCollectionFrames);

    SIZEOF(StartActivity);
    OFFSETOF(StartActivity, startTime);

    SIZEOF(SignalPath);
    OFFSETOF(SignalPath, rfFreq);
    OFFSETOF(SignalPath, drift);
    OFFSETOF(SignalPath, width);
    OFFSETOF(SignalPath, power);

    SIZEOF(SignalId);
    OFFSETOF(SignalId, dxNumber);
    OFFSETOF(SignalId, activityId);
    OFFSETOF(SignalId, activityStartTime);
    OFFSETOF(SignalId, number);

    SIZEOF(SignalDescription);
    OFFSETOF(SignalDescription, path);
    OFFSETOF(SignalDescription, pol);
    OFFSETOF(SignalDescription, sigClass);
    OFFSETOF(SignalDescription, reason); 
    OFFSETOF(SignalDescription, subchannelNumber);
    OFFSETOF(SignalDescription, signalId);
    OFFSETOF(SignalDescription, origSignalId);

    SIZEOF(CwPowerSignal);
    OFFSETOF(CwPowerSignal, sig);

    SIZEOF(ConfirmationStats);
    OFFSETOF(ConfirmationStats, pfa); 
    OFFSETOF(ConfirmationStats, snr);

    SIZEOF(CwCoherentSegment);
    OFFSETOF(CwCoherentSegment, path);
    OFFSETOF(CwCoherentSegment, pfa);
    OFFSETOF(CwCoherentSegment, snr);

    SIZEOF(CwCoherentSignal);
    OFFSETOF(CwCoherentSignal, sig);
    OFFSETOF(CwCoherentSignal, cfm);
    OFFSETOF(CwCoherentSignal, nSegments);
    OFFSETOF(CwCoherentSignal, segment);

    SIZEOF(Pulse);    
    OFFSETOF(Pulse, rfFreq);
    OFFSETOF(Pulse, power);
    OFFSETOF(Pulse, spectrumNumber);
    OFFSETOF(Pulse, binNumber);
    OFFSETOF(Pulse, pol);

    SIZEOF(PulseTrainDescription); 
    OFFSETOF(PulseTrainDescription, pulsePeriod);
    OFFSETOF(PulseTrainDescription, numberOfPulses);
    OFFSETOF(PulseTrainDescription, res);

    SIZEOF(PulseSignalHeader);
    OFFSETOF(PulseSignalHeader, sig);
    OFFSETOF(PulseSignalHeader, cfm);
    OFFSETOF(PulseSignalHeader, train);

    SIZEOF(FollowUpSignal);
    OFFSETOF(FollowUpSignal, rfFreq);
    OFFSETOF(FollowUpSignal, drift);
    OFFSETOF(FollowUpSignal, res);
    OFFSETOF(FollowUpSignal, origSignalId);

    SIZEOF(FollowUpCwSignal);
    OFFSETOF(FollowUpCwSignal, sig);

    SIZEOF(FollowUpPulseSignal);
    OFFSETOF(FollowUpPulseSignal, sig);


    SIZEOF(DetectionStatistics);
    OFFSETOF(DetectionStatistics, totalCandidates);
    OFFSETOF(DetectionStatistics, cwCandidates);
    OFFSETOF(DetectionStatistics, pulseCandidates);
    OFFSETOF(DetectionStatistics, candidatesOverMax);
    OFFSETOF(DetectionStatistics, totalSignals);
    OFFSETOF(DetectionStatistics, cwSignals);
    OFFSETOF(DetectionStatistics, pulseSignals);
    OFFSETOF(DetectionStatistics, leftCwHits);
    OFFSETOF(DetectionStatistics, rightCwHits);
    OFFSETOF(DetectionStatistics, leftCwClusters);
    OFFSETOF(DetectionStatistics, rightCwClusters);
    OFFSETOF(DetectionStatistics, totalPulses);
    OFFSETOF(DetectionStatistics, leftPulses);
    OFFSETOF(DetectionStatistics, rightPulses);
    OFFSETOF(DetectionStatistics, triplets);
    OFFSETOF(DetectionStatistics, pulseTrains);
    OFFSETOF(DetectionStatistics, pulseClusters);

    SIZEOF(CwBadBand);
    OFFSETOF(CwBadBand, band);
    OFFSETOF(CwBadBand, pol);
    OFFSETOF(CwBadBand, paths);
    OFFSETOF(CwBadBand, maxPathCount);
    OFFSETOF(CwBadBand, maxPath);

    SIZEOF(PulseBadBand);
    OFFSETOF(PulseBadBand, band);
    OFFSETOF(PulseBadBand, res);
    OFFSETOF(PulseBadBand, pol);
    OFFSETOF(PulseBadBand, pulses);
    OFFSETOF(PulseBadBand, maxPulseCount);
    OFFSETOF(PulseBadBand, triplets);
    OFFSETOF(PulseBadBand, maxTripletCount);
    OFFSETOF(PulseBadBand, tooManyTriplets);


    SIZEOF(BaselineHeader);
    OFFSETOF(BaselineHeader, rfCenterFreq);
    OFFSETOF(BaselineHeader, bandwidth);
    OFFSETOF(BaselineHeader, halfFrameNumber);
    OFFSETOF(BaselineHeader, numberOfSubchannels);
    OFFSETOF(BaselineHeader, pol);

    SIZEOF(Baseline);
    OFFSETOF(Baseline, header);
    OFFSETOF(Baseline, baselineValues);

    SIZEOF(ComplexAmplitudeHeader);
    OFFSETOF(ComplexAmplitudeHeader, rfCenterFreq);
    OFFSETOF(ComplexAmplitudeHeader, halfFrameNumber);
    OFFSETOF(ComplexAmplitudeHeader, hzPerSubchannel);
    OFFSETOF(ComplexAmplitudeHeader, startSubchannelId);
    OFFSETOF(ComplexAmplitudeHeader, numberOfSubchannels);
	//This structure no longer has a res
    //OFFSETOF(ComplexAmplitudeHeader, res);
    OFFSETOF(ComplexAmplitudeHeader, pol);

    SIZEOF(ComplexAmplitudes);
    OFFSETOF(ComplexAmplitudes, header);
    OFFSETOF(ComplexAmplitudes, compamp);

    SIZEOF(ArchiveRequest);
    OFFSETOF(ArchiveRequest, signalId);
    
    SIZEOF(ArchiveDataHeader);
    OFFSETOF(ArchiveDataHeader, signalId);
    

    SIZEOF(DxActivityStatus);
    OFFSETOF(DxActivityStatus, activityId); 
    OFFSETOF(DxActivityStatus, currentState);   

    SIZEOF(DxStatus);
    OFFSETOF(DxStatus, timestamp);   
    OFFSETOF(DxStatus, numberOfActivities); 
    OFFSETOF(DxStatus, act);

    SIZEOF(Count);
    OFFSETOF(Count, count);

    SIZEOF(BaselineStatistics);
    OFFSETOF(BaselineStatistics, mean);
    OFFSETOF(BaselineStatistics, stdDev);
    OFFSETOF(BaselineStatistics, range);
    OFFSETOF(BaselineStatistics, halfFrameNumber);
    OFFSETOF(BaselineStatistics, rfCenterFreqMhz);
    OFFSETOF(BaselineStatistics, bandwidthMhz);
    OFFSETOF(BaselineStatistics, pol);
    OFFSETOF(BaselineStatistics, status);
    
    SIZEOF(BaselineLimitsExceededDetails);
    OFFSETOF(BaselineLimitsExceededDetails, pol);
    OFFSETOF(BaselineLimitsExceededDetails, description);

}
