/*******************************************************************************

 File:    TscopeProxy.cpp
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
#include "DebugLog.h" // keep this early in the headers for VERBOSE macros
#include "TscopeProxy.h"
#include "NssComponentManager.h"
#include "ObserveActivity.h"
#include "SseTscopeMsg.h"
#include "SseMsg.h"

using namespace std;

TscopeProxy::TscopeProxy(NssComponentManager<TscopeProxy> *siteTscopeManager)
   : 
   siteTscopeManager_(siteTscopeManager), 
   activity_(0)
{
   SseUtil::strMaxCpy(intrinsics_.name, "tscope-unknown", MAX_TEXT_STRING);
}

TscopeProxy::~TscopeProxy()
{
   VERBOSE2(getVerboseLevel(), "TscopeProxy destructor for " << getName() 
	    << endl;);
}

TscopeStatusMultibeam TscopeProxy::getStatus() const 
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(statusMutex_);

   return status_;
}

void TscopeProxy::setStatus(const TscopeStatusMultibeam & status)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(statusMutex_);

   status_ = status;

   VERBOSE3(getVerboseLevel(), status_);
}


TscopeIntrinsics TscopeProxy::getIntrinsics() const
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(intrinsicsMutex_);

   return intrinsics_;
}

void TscopeProxy::setIntrinsics(const TscopeIntrinsics & intrinsics)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(intrinsicsMutex_);

   intrinsics_ = intrinsics;

   VERBOSE2(getVerboseLevel(), intrinsics_);
}

string TscopeProxy::getName() 
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(intrinsicsMutex_);

   return intrinsics_.name;
}

void TscopeProxy::setName(const string & name) 
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(intrinsicsMutex_);

   SseUtil::strMaxCpy(intrinsics_.name, name.c_str(), MAX_TEXT_STRING);
}

void TscopeProxy::attachObserveActivity(ObserveActivity* obsAct)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activityPtrMutex_);
   
   activity_ = obsAct;
}

void TscopeProxy::detachObserveActivity() 
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activityPtrMutex_);

   activity_ = 0;
}

string TscopeProxy::expectedInterfaceVersion() 
{
   return SSE_TSCOPE_INTERFACE_VERSION;
}

string TscopeProxy::receivedInterfaceVersion()
{ 
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(intrinsicsMutex_);

   return intrinsics_.interfaceVersionNumber;
}

void TscopeProxy::allocate(const string & subarray) 
{
   TscopeMessageCode code = TSCOPE_ALLOCATE;
   TscopeSubarray msgBody;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::deallocate(const string & subarray)
{
   TscopeMessageCode code = TSCOPE_DEALLOCATE;
   TscopeSubarray msgBody;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::connect()
{
   TscopeMessageCode code = TSCOPE_CONNECT;
   sendMsgNoId(code);
}

void TscopeProxy::disconnect()
{
   TscopeMessageCode code = TSCOPE_DISCONNECT;
   sendMsgNoId(code);
}


void TscopeProxy::monitor(int periodSecs)
{
   VERBOSE1(getVerboseLevel(), "Monitor: " << periodSecs << endl;);

   TscopeMessageCode code = TSCOPE_MONITOR;
   TscopeMonitorRequest msgBody;
   msgBody.periodSecs = periodSecs;

   int dataLength  = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);

}

void TscopeProxy::assignSubarray(const TscopeAssignSubarray &assign)
{
   VERBOSE1(getVerboseLevel(), "assign subarray:" << endl;);
   VERBOSE2(getVerboseLevel(), assign);

   TscopeMessageCode code = TSCOPE_BF_SET_ANTS;
   TscopeAssignSubarray msgBody = assign;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}


void TscopeProxy::beamformerStop() 
{
   TscopeMessageCode code = TSCOPE_BF_STOP;
   sendMsgNoId(code);
}

void TscopeProxy::beamformerReset() 
{
   TscopeMessageCode code = TSCOPE_BF_RESET;
   sendMsgNoId(code);
}

void TscopeProxy::beamformerInit() 
{
   TscopeMessageCode code = TSCOPE_BF_INIT;
   sendMsgNoId(code);
}

void TscopeProxy::beamformerAutoatten() 
{
   TscopeMessageCode code = TSCOPE_BF_AUTOATTEN;
   sendMsgNoId(code);
}


//JR - Added to support sending destination to bfinit.
void TscopeProxy::beamformerDest(string cmdWithArgs)
{
   VERBOSE1(getVerboseLevel(), "beamformer Dest:" << endl;);
   VERBOSE1(getVerboseLevel(), cmdWithArgs);

   TscopeMessageCode code = TSCOPE_BF_DEST;
   TscopeBackendCmd msgBody;
   SseUtil::strMaxCpy(msgBody.cmdWithArgs, cmdWithArgs.c_str(),
                      MAX_TEXT_STRING);
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::beginSendingCommandSequence() 
{
   TscopeMessageCode code = TSCOPE_BEGIN_SENDING_COMMAND_SEQUENCE;
   sendMsgNoId(code);
}

void TscopeProxy::doneSendingCommandSequence() 
{
   TscopeMessageCode code = TSCOPE_DONE_SENDING_COMMAND_SEQUENCE;
   sendMsgNoId(code);
}

void TscopeProxy::pointSubarray(const TscopeSubarrayCoords& coords)
{
   VERBOSE1(getVerboseLevel(), "Point subarray:" << endl;);
   VERBOSE2(getVerboseLevel(), coords);

   TscopeMessageCode code = TSCOPE_POINT_SUBARRAY;
   TscopeSubarrayCoords msgBody = coords;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::beamformerSetBeamCoords(const TscopeBeamCoords& coords)
{
   VERBOSE1(getVerboseLevel(), "beamformerSetBeamCoords:" << endl;);
   VERBOSE2(getVerboseLevel(), coords);

   TscopeMessageCode code = TSCOPE_BF_SET_COORDS;
   TscopeBeamCoords msgBody = coords;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::beamformerAddNullBeamCoords(const TscopeBeamCoords& coords)
{
   VERBOSE1(getVerboseLevel(), "beamformerAddNullBeamCoords:" << endl;);
   VERBOSE2(getVerboseLevel(), coords);

   TscopeMessageCode code = TSCOPE_BF_ADD_NULL;
   TscopeBeamCoords msgBody = coords;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}


void TscopeProxy::beamformerSetNullType(const TscopeNullType& nullType)
{
   VERBOSE1(getVerboseLevel(), "beamformerSetNullType:" << endl;);
   VERBOSE2(getVerboseLevel(), nullType);

   TscopeMessageCode code = TSCOPE_BF_SET_NULL_TYPE;
   TscopeNullType msgBody = nullType;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::beamformerClearBeamNulls() 
{
   TscopeMessageCode code = TSCOPE_BF_CLEAR_NULLS;
   sendMsgNoId(code);
}

void TscopeProxy::beamformerPoint() 
{
   TscopeMessageCode code = TSCOPE_BF_POINT;
   sendMsgNoId(code);
}

void TscopeProxy::beamformerClearCoords() 
{
   TscopeMessageCode code = TSCOPE_BF_CLEAR_COORDS;
   sendMsgNoId(code);
}

void TscopeProxy::beamformerClearAnts() 
{
   TscopeMessageCode code = TSCOPE_BF_CLEAR_ANTS;
   sendMsgNoId(code);
}

void TscopeProxy::beamformerCal(const TscopeCalRequest& request)
{
   VERBOSE1(getVerboseLevel(), "Beamformer cal:" << endl;);
   VERBOSE2(getVerboseLevel(), request);

   TscopeMessageCode code = TSCOPE_BF_CAL;
   TscopeCalRequest msgBody = request;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::requestPointingCheck(const TscopeSubarrayCoords& coords)
{
   VERBOSE1(getVerboseLevel(), "Request pointing check:" << endl;);
   VERBOSE2(getVerboseLevel(), coords);

   TscopeMessageCode code = TSCOPE_REQUEST_POINT_CHECK;
   TscopeSubarrayCoords msgBody = coords;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}


void TscopeProxy::requestIntrinsics() 
{
   TscopeMessageCode code = TSCOPE_REQUEST_INTRINSICS;
   sendMsgNoId(code);
}

void TscopeProxy::reset()
{
   TscopeMessageCode code = TSCOPE_RESET;
   sendMsgNoId(code);
}


void TscopeProxy::tune(const TscopeTuneRequest &request)
{
   TscopeMessageCode code = TSCOPE_TUNE;
   TscopeTuneRequest msgBody;
   msgBody = request;

   VERBOSE1(getVerboseLevel(), "Tune Telescope Sky Freq:" << endl;);
   VERBOSE2(getVerboseLevel(), msgBody);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);

}


void TscopeProxy::shutdown()
{
   TscopeMessageCode code = TSCOPE_SHUTDOWN;
   sendMsgNoId(code);
}

void TscopeProxy::requestStatusUpdate()
{
   TscopeMessageCode code = TSCOPE_REQUEST_STATUS;
   sendMsgNoId(code);
}

void TscopeProxy::stop(const string& subarray)
{
   TscopeMessageCode code = TSCOPE_STOP;
   TscopeStopRequest msgBody;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::stow(const string& subarray)
{
   TscopeMessageCode code = TSCOPE_STOW;
   TscopeStowRequest msgBody;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::wrap(const string& subarray, int wrapNumber)
{
   VERBOSE1(getVerboseLevel(), "wrap: " << wrapNumber << endl;);

   TscopeMessageCode code = TSCOPE_WRAP;
   TscopeWrapRequest msgBody;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);
   msgBody.wrapNumber = wrapNumber;

   int dataLength  = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::lnaOn(const string& subarray)
{
   TscopeMessageCode code = TSCOPE_LNA_ON;
   TscopeLnaOnRequest msgBody;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::pamSet(const string& subarray)
{
   TscopeMessageCode code = TSCOPE_PAM_SET;
   TscopePamSetRequest msgBody;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}


void TscopeProxy::simulate()
{
   TscopeMessageCode code = TSCOPE_SIMULATE;
   sendMsgNoId(code);
}

void TscopeProxy::unsimulate()
{
   TscopeMessageCode code = TSCOPE_UNSIMULATE;
   sendMsgNoId(code);
}

void TscopeProxy::zfocus(const string& subarray, double skyFreqMhz)
{
   TscopeMessageCode code = TSCOPE_ZFOCUS;
   TscopeZfocusRequest msgBody;
   msgBody.skyFreqMhz = skyFreqMhz;
   SseUtil::strMaxCpy(msgBody.subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   VERBOSE1(getVerboseLevel(), "Tscope zfocus" << endl;);
   VERBOSE2(getVerboseLevel(), msgBody);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);

}

void TscopeProxy::sendBackendCommand(const string & cmdWithArgs)
{
   VERBOSE1(getVerboseLevel(), "sendBackendCommand:" << endl;);
   VERBOSE1(getVerboseLevel(), cmdWithArgs);

   TscopeMessageCode code = TSCOPE_SEND_BACKEND_CMD;
   TscopeBackendCmd msgBody;
   SseUtil::strMaxCpy(msgBody.cmdWithArgs, cmdWithArgs.c_str(),
                      MAX_TEXT_STRING);
   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}

void TscopeProxy::antgroupAutoselect(const string & bflist) 
{
   TscopeMessageCode code = TSCOPE_ANTGROUP_AUTOSELECT;

   TscopeAntgroupAutoselect msgBody;
   //msgBody.maxSefdJy = maxSefdJy;
   //msgBody.obsFreqMhz = obsFreqMhz;
   SseUtil::strMaxCpy(msgBody.bflist, bflist.c_str(),
                      MAX_TEXT_STRING);

   int dataLength = sizeof(msgBody);
   msgBody.marshall();
   sendMsgNoId(code, dataLength, &msgBody);
}


// *** utility routines ****
//---------------------------------------------

// send a marshalled message header to the TSCOPE


void TscopeProxy::sendMsgNoId(int messageCode, int dataLength, 
			      const void *msgBody)
{
   int activityId = NSS_NO_ACTIVITY_ID;
   sendMessage(messageCode, activityId, dataLength, msgBody);
}

void TscopeProxy::sendMessage(int messageCode, int activityId, int dataLength, 
			      const void *msgBody)
{
   VERBOSE2(getVerboseLevel(), 
	    "Send " << getName() <<  ": "
	    << SseTscopeMsg::getMessageCodeString(messageCode) << "\n");

   // Assume this is mutex protected by NssProxy
   NssProxy::sendMessage(messageCode, activityId, dataLength, msgBody);
}

void TscopeProxy::notifyInputConnected()
{
   // Register this device with the site.
   // Need to do this here instead of in the Proxy constructor
   // because we don't want to register the proxy until its
   // socket connection with the real device is established.

   siteTscopeManager_->registerProxy(this);
}

void TscopeProxy::sendDisconnectErrorToActivity()
{ 
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activityPtrMutex_);
   
   // notify the activity that we're gone
   if (activity_)
   {
      NssMessage nssMessage(TSCOPE_ERROR, SEVERITY_ERROR, 
			    "Tscope (telescope) disconnected\n");
      activity_->tscopeError(this, nssMessage);
   }
}

void TscopeProxy::sendTscopeReadyToActivity()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activityPtrMutex_);

   if (activity_)
   {
      activity_->tscopeReady(this);
   }
}
void TscopeProxy::sendTscopeCancelTimerToActivity()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activityPtrMutex_);

   if (activity_)
   {
      activity_->tscopeCancelTimer(this);
   }
}

void TscopeProxy::notifyInputDisconnected()
{
   sendDisconnectErrorToActivity();

   siteTscopeManager_->unregisterProxy(this);
}

void TscopeProxy::forwardErrorToActivity(NssMessage & nssMessage)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activityPtrMutex_);
   
   if (activity_)
   {
      activity_->tscopeError(this, nssMessage);
   }
}

// dispatch incoming tscope message to appropriate
// sse classes & methods
void TscopeProxy::handleIncomingMessage(SseInterfaceHeader *hdr, 
					void *bodybuff)
{
   VERBOSE2(getVerboseLevel(),
	    "Recv " << getName() <<  ": "
	    << SseTscopeMsg::getMessageCodeString(hdr->code) << "\n" << *hdr);

   NssMessage *nssMessagePtr;
   NssMessage nssMessage;
   TscopeStatusMultibeam *statusMultibeamPtr;
   TscopeIntrinsics *intrinsicsPtr;

   // dispatch here
   // process message body, if there is one
   switch (hdr->code)
   {

   case TSCOPE_ERROR:
      nssMessagePtr = static_cast<NssMessage *>(bodybuff);
      nssMessagePtr->demarshall();
      //cerr << *nssMessagePtr; // TBD

      forwardErrorToActivity(*nssMessagePtr);
      siteTscopeManager_->processNssMessage(this, *nssMessagePtr);

      break;

   case TSCOPE_MESSAGE:
      nssMessagePtr = static_cast<NssMessage *>(bodybuff);
      nssMessagePtr->demarshall();
      siteTscopeManager_->processNssMessage(this, *nssMessagePtr);

      break;

   case TSCOPE_STATUS_MULTIBEAM:
      statusMultibeamPtr = static_cast<TscopeStatusMultibeam*>(bodybuff);
      statusMultibeamPtr->demarshall();
      setStatus(*statusMultibeamPtr);
      siteTscopeManager_->notifyStatusChanged(this);

      break;

   case TSCOPE_INTRINSICS:
      intrinsicsPtr = static_cast<TscopeIntrinsics*>(bodybuff);
      intrinsicsPtr->demarshall();
      setIntrinsics(*intrinsicsPtr);
      siteTscopeManager_->receiveIntrinsics(this);

      break;

   case TSCOPE_TRACKING_ON:

      VERBOSE2(getVerboseLevel(), "Tracking on.");
      // do nothing
      break;

   case TSCOPE_TRACKING_OFF:

      VERBOSE2(getVerboseLevel(), "Tracking off.");

      nssMessage.code = 0; // TBD
      nssMessage.severity = SEVERITY_WARNING;
      SseUtil::strMaxCpy(nssMessage.description,"Tracking Off\n", MAX_TEXT_STRING);

      forwardErrorToActivity(nssMessage);
      siteTscopeManager_->processNssMessage(this, nssMessage);

      break;


   case TSCOPE_READY:

      VERBOSE2(getVerboseLevel(), "Tscope ready.");
      sendTscopeReadyToActivity();

      break;

   case TSCOPE_CANCEL_TIMER:

      VERBOSE2(getVerboseLevel(), "Tscope Cancel Timer.");
      sendTscopeCancelTimerToActivity();

      break;

   default: 
      VERBOSE2(getVerboseLevel(), 
	       "TscopeProxy::handleTscopeMessage: "
	       << "Tscope unexpected message code received: " 
	       << hdr->code << endl;);
      break;
   };  


}
