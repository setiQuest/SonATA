/*******************************************************************************

 File:    DxProxy.h
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


#ifndef _dx_proxy_h
#define _dx_proxy_h

#include "sseDxInterface.h"
#include "SharedProxy.h"
#include <string>

class ActivityUnit;
template<typename Tproxy> class NssComponentManager;
class DxProxyInternal;

class DxProxy : public SharedProxy
{

public:
    DxProxy(NssComponentManager<DxProxy> *siteDxManager);
    ~DxProxy();

    // public utility methods
    // -----------------------
    void notifyInputConnected();
    void notifyInputDisconnected();

    void attachActivityUnit(ActivityUnit *actUnit);
    void detachActivityUnit(ActivityUnit *actUnit);

    DxIntrinsics getIntrinsics();
    DxConfiguration getConfiguration();
    DxStatus getCachedDxStatus();
    string getName();
    int getNumber();
    void setName(const string &name);

    void setDxSkyFreq(float64_t skyfreq);
    float64_t getDxSkyFreq();

    void setChannelNumber(int32_t chanNumber);
    int32_t getChannelNumber();

    float64_t getBandwidthInMHz() const;
    void setIntrinsicsBandwidth(int32_t maxSubchannels, float64_t hzPerSubchannel);

    // outgoing messages to Dx
    // ------------------------
    void requestIntrinsics();
    void configureDx(const DxConfiguration &config);
    void sendPermRfiMask(const FrequencyMaskHeader &mask,
			const FrequencyBand freqBandArray[]);
    void sendBirdieMask(const FrequencyMaskHeader &mask,
			const FrequencyBand freqBandArray[]);
    void sendRcvrBirdieMask(const FrequencyMaskHeader &mask,
			const FrequencyBand freqBandArray[]);
    void sendTestSignalMask(const FrequencyMaskHeader &mask,
			const FrequencyBand freqBandArray[]);
    void sendRecentRfiMask(const RecentRfiMaskHeader &mask,
			const FrequencyBand freqBandArray[]);
    void requestStatusUpdate();
    void sendDxActivityParameters(const DxActivityParameters &actParam);
    void dxScienceDataRequest(const DxScienceDataRequest &scienceDataRequest);
    void setStartTime(int activityId, const StartActivity &startAct);
    void stopDxActivity(int activityId);
    void shutdown();
    void restart();

    void requestArchiveData(int activityId, 
			    const ArchiveRequest &archiveRequest);

    void discardArchiveData(int activityId,
			    const ArchiveRequest &archiveRequest);

    // outgoing messages to ZXs
    void beginSendingRequestedCompampSubchannels(int activityId, const Count &nSubchannels);
    void sendRequestedCompampSubchannel(int activityId, const int subchannelNumber);
    void doneSendingRequestedCompampSubchannels(int activityId);

    // outgoing messages
    // -----------------
    void beginSendingFollowUpSignals(int activityId, const Count &nSignals);
    void sendFollowUpCwSignal(int activityId, 
			      const FollowUpCwSignal &followUpCwSignal);
    void sendFollowUpPulseSignal(int activityId, 
				 const FollowUpPulseSignal &followUpPulseSignal);
    void doneSendingFollowUpSignals(int activityId);


    // outgoing messages for secondary processing
    void beginSendingCandidatesSecondary(int activityId, const Count &count);
    void sendCandidateCwPowerSignalSecondary(int activityId,
					       const CwPowerSignal &cwPowerSignal);
    void sendCandidatePulseSignalSecondary(int activityId, const PulseSignalHeader &hdr,
					     Pulse pulses[]);
    void doneSendingCandidatesSecondary(int activityId);

    void beginSendingCwCoherentSignalsSecondary(int activityId, const Count &count);
    void sendCwCoherentSignalSecondary(int activityId, 
					 const CwCoherentSignal &cwCoherentSignal);
    void doneSendingCwCoherentSignalsSecondary(int activityId);


private:
    // Disable copy construction & assignment.
    // Don't define these.
    DxProxy(const DxProxy& rhs);
    DxProxy& operator=(const DxProxy& rhs);

    DxProxyInternal *internal_;

    friend class DxProxyInternal;

    // ----- private utilities -----------------------
    void sendMsgNoId(int messageCode, int dataLength = 0,
		     const void *msgBody = 0);

    // from base class
    void sendMessage(int messageCode, int activityId = -1, int dataLength = 0,
		     const void *msgBody = 0);

    string expectedInterfaceVersion();
    string receivedInterfaceVersion();

    void handleIncomingMessage(SseInterfaceHeader *hdr, void *bodybuff);

};


#endif // _dx_proxy_h
