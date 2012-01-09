/*******************************************************************************

 File:    TscopeProxy.h
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


#ifndef _tscope_proxy_h
#define _tscope_proxy_h

#include "ace/Synch.h"
#include "SharedProxy.h"
#include "sseTscopeInterface.h"
#include <string>

class ObserveActivity;

template<typename Tproxy> class NssComponentManager;

class TscopeProxy : public SharedProxy
{
 public:

   TscopeProxy(NssComponentManager<TscopeProxy> *siteTscopeManager);
   ~TscopeProxy();

   TscopeStatusMultibeam getStatus() const;
   TscopeIntrinsics getIntrinsics() const;
   string getName();
   void setName(const string &name);

   // utilities
   void notifyInputConnected();
   void notifyInputDisconnected();
   void attachObserveActivity(ObserveActivity* obsAct);
   void detachObserveActivity();

   // outgoing messages to Tscope

   void beginSendingCommandSequence();
   void doneSendingCommandSequence();

   void antgroupAutoselect(const string & bflist);
   void assignSubarray(const TscopeAssignSubarray &assign);
   void allocate(const string & subarray);
   void deallocate(const string & subarray);
   void connect();
   void disconnect();
   void monitor(int periodSecs);
   void pointSubarray(const TscopeSubarrayCoords& coords);
   void requestPointingCheck(const TscopeSubarrayCoords& coords);
   void requestIntrinsics();
   void reset();
   void requestStatusUpdate();
   void tune(const TscopeTuneRequest& request);
   void shutdown();
   void stop(const string & subarray);
   void stow(const string & subarray);
   void wrap(const string & subarray, int wrapNumber);
   void lnaOn(const string & subarray);
   void pamSet(const string & subarray);
   void zfocus(const string & subarray, double skyFreqMhz);
   void sendBackendCommand(const string & cmdWithArgs);

   void simulate();
   void unsimulate();

   void beamformerInit();
   void beamformerDest(string cmdWithArgs); //JR - Added to support telling bfinit the destination 
   void beamformerReset();
   void beamformerAutoatten();
   void beamformerSetBeamCoords(const TscopeBeamCoords& coords);
   void beamformerClearCoords();
   void beamformerClearAnts();
   void beamformerSetNullType(const TscopeNullType& nullType);
   void beamformerClearBeamNulls();
   void beamformerAddNullBeamCoords(const TscopeBeamCoords& coords);
   void beamformerPoint();
   void beamformerCal(const TscopeCalRequest &request);
   void beamformerStop();


 private:

   // Disable copy construction & assignment.
   // Don't define these.
   TscopeProxy(const TscopeProxy& rhs);
   TscopeProxy& operator=(const TscopeProxy& rhs);

   // utilities
   void sendMessage(int messageCode, int activityId = NSS_NO_ACTIVITY_ID, 
		    int dataLength = 0, const void *msgBody = 0);
   void sendMsgNoId(int messageCode, int dataLength = 0, 
		    const void *msgBody = 0);

   string expectedInterfaceVersion();
   string receivedInterfaceVersion();

   void handleIncomingMessage(SseInterfaceHeader *hdr, void *bodybuff);

   void sendDisconnectErrorToActivity();
   void forwardErrorToActivity(NssMessage & nssMessage);
   void sendTscopeReadyToActivity();
   void sendTscopeCancelTimerToActivity();

   void setStatus(const TscopeStatusMultibeam & status);
   void setIntrinsics(const TscopeIntrinsics & intrinsics);

   NssComponentManager<TscopeProxy>* siteTscopeManager_;
   ObserveActivity *activity_;
   TscopeStatusMultibeam status_;
   TscopeIntrinsics intrinsics_;

   ACE_Recursive_Thread_Mutex activityPtrMutex_; 
   mutable ACE_Recursive_Thread_Mutex rfSkyFreqMutex_; 
   mutable ACE_Recursive_Thread_Mutex statusMutex_; 
   mutable ACE_Recursive_Thread_Mutex intrinsicsMutex_; 

};

#endif // _tscope_proxy_h
