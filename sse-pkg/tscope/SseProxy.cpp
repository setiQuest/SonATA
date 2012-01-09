/*******************************************************************************

 File:    SseProxy.cpp
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
#include <iostream>

#include "SseProxy.h"
#include "Tscope.h"
#include "SseTscopeMsg.h"
#include "Verbose.h"
#include "TscopeEventHandler.h"

using namespace std;

SseProxy::SseProxy(ACE_SOCK_STREAM &stream) :
   NssProxy(stream), tscope_(0), verboseLevel_(0)
{
}
  
void SseProxy::setTscope(Tscope* tscope)
{
   tscope_ = tscope;
}

Tscope* SseProxy::getTscope()
{
   return tscope_;
}

void SseProxy::setVerbose(int level)
{
   verboseLevel_ = level;
}


string SseProxy::getName()
{
   return "sse (from tscope)";
}


void SseProxy::sendErrorMsgToSse(NssMessageSeverity severity, 
				 const string &text)
{
   VERBOSE1(verboseLevel_, text);

   NssMessage nssMsg;
   nssMsg.code = TSCOPE_ERROR;
   nssMsg.severity = severity;
   SseUtil::strMaxCpy(nssMsg.description, text.c_str(), 
                      MAX_NSS_MESSAGE_STRING);
   
   int dataLength = sizeof(nssMsg);
   nssMsg.marshall();
   
   TscopeMessageCode code = TSCOPE_ERROR;
   int activityId = NSS_NO_ACTIVITY_ID;
   
   sendMessage(code, activityId, dataLength, &nssMsg);
}


void SseProxy::sendMsgToSse(NssMessageSeverity severity, 
                            const string &text)
{
   VERBOSE1(verboseLevel_, text);

   NssMessage nssMsg;
   nssMsg.code = TSCOPE_MESSAGE;
   nssMsg.severity = severity;
   SseUtil::strMaxCpy(nssMsg.description, text.c_str(), 
                      MAX_NSS_MESSAGE_STRING);
   
   int dataLength = sizeof(nssMsg);
   nssMsg.marshall();
   
   TscopeMessageCode code = TSCOPE_MESSAGE;
   int activityId = NSS_NO_ACTIVITY_ID;
   
   sendMessage(code, activityId, dataLength, &nssMsg);
}

void SseProxy::sendStatus(const TscopeStatusMultibeam& status) 
{
   VERBOSE2(verboseLevel_, "Send TSCOPE status (multibeam):\n";);
   VERBOSE3(verboseLevel_, status;);

   TscopeMessageCode code = TSCOPE_STATUS_MULTIBEAM;
   TscopeStatusMultibeam msgBody = status;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();

   int activityId = NSS_NO_ACTIVITY_ID;
   sendMessage(code, activityId, dataLength, &msgBody);
}

void SseProxy::sendIntrinsics(const TscopeIntrinsics& intrinsics) 
{
   VERBOSE2(verboseLevel_,  "Intrinsics:\n" << intrinsics;);

   TscopeMessageCode code = TSCOPE_INTRINSICS;
   TscopeIntrinsics msgBody = intrinsics;
   int dataLength = sizeof(msgBody);
   msgBody.marshall();

   int activityId = NSS_NO_ACTIVITY_ID;
   sendMessage(code, activityId, dataLength, &msgBody);
}

void SseProxy::trackingOn() 
{
   TscopeMessageCode code = TSCOPE_TRACKING_ON;
   sendMessage(code);
}

void SseProxy::trackingOff() 
{
   TscopeMessageCode code = TSCOPE_TRACKING_OFF;
   sendMessage(code);
}

void SseProxy::ready() 
{
   TscopeMessageCode code = TSCOPE_READY;
   sendMessage(code);
}

void SseProxy::cancelTimer() 
{
   TscopeMessageCode code = TSCOPE_CANCEL_TIMER;
   sendMessage(code);
}

// --- utilities -----

// send a marshalled message to the SSE
void SseProxy::sendMessage(int messageCode, int activityId,
			   int dataLength, const void *msgBody)
{
   VERBOSE2(verboseLevel_,  "Send SSE: " << 
            SseTscopeMsg::getMessageCodeString(messageCode) << endl;);
   NssProxy::sendMessage(messageCode, activityId, dataLength, msgBody);
}

// dispatch incoming sse message to appropriate tscope method
void SseProxy::handleIncomingMessage(SseInterfaceHeader *hdr, void *bodybuff)
{
  
   VERBOSE2(verboseLevel_,  "\nRecv SSE: " 
            << SseTscopeMsg::getMessageCodeString(hdr->code) << endl;);

   TscopeAssignSubarray *assignSub;
   TscopeBackendCmd *destReq;
   TscopeCalRequest *calReq;
   TscopeMonitorRequest *monitorReq;
   TscopeStopRequest *stopReq;
   TscopeStowRequest *stowReq;
   TscopeBeamCoords *beamCoords;
   TscopeNullType *nullType;
   TscopeSubarrayCoords *subarrayCoords;
   TscopeTuneRequest *tuneReq;
   TscopeWrapRequest *wrapReq;
   TscopeZfocusRequest *zfocusReq;
   TscopeLnaOnRequest *lnaOnReq;
   TscopePamSetRequest *pamSetReq;
   TscopeBackendCmd *backendCmd;
   TscopeSubarray *subarray;
   TscopeAntgroupAutoselect *antAuto;

   switch (hdr->code)
   {

   case TSCOPE_ALLOCATE:
      subarray = static_cast<TscopeSubarray*> (bodybuff);
      subarray->demarshall();
      tscope_->allocate(*subarray);
      break;

   case TSCOPE_ANTGROUP_AUTOSELECT:
      antAuto = static_cast<TscopeAntgroupAutoselect*> (bodybuff);
      antAuto->demarshall();
      tscope_->antgroupAutoselect(*antAuto);
      break;

   case TSCOPE_BF_SET_ANTS:
      assignSub = static_cast<TscopeAssignSubarray*> (bodybuff);
      assignSub->demarshall();
      tscope_->assignSubarray(*assignSub);
      break;

   case TSCOPE_BF_STOP:
      tscope_->beamformerStop();
      break;

   case TSCOPE_BF_RESET:
      tscope_->beamformerReset();
      break;

   case TSCOPE_BF_INIT:
      tscope_->beamformerInit();
      break;

   //JR - Added to support telling bfinit the destinations
   case TSCOPE_BF_DEST:
      destReq = static_cast<TscopeBackendCmd*> (bodybuff);
      destReq->demarshall();
      tscope_->beamformerDest(*destReq);
      break;

   case TSCOPE_BF_AUTOATTEN:
      tscope_->beamformerAutoatten();
      break;

   case TSCOPE_BF_CAL:
      calReq = static_cast<TscopeCalRequest*> (bodybuff);
      calReq->demarshall();
      tscope_->beamformerCal(*calReq);
      break;

   case TSCOPE_CONNECT:
      tscope_->connect();
      break;

   case TSCOPE_DEALLOCATE:
      subarray = static_cast<TscopeSubarray*> (bodybuff);
      subarray->demarshall();
      tscope_->deallocate(*subarray);
      break;

   case TSCOPE_DISCONNECT:
      tscope_->disconnect();
      break;

   case TSCOPE_MONITOR:
      monitorReq = static_cast<TscopeMonitorRequest*> (bodybuff);
      monitorReq->demarshall();
      tscope_->monitor(*monitorReq);
      break;

   case TSCOPE_POINT_SUBARRAY:
      subarrayCoords = static_cast<TscopeSubarrayCoords*> (bodybuff);
      subarrayCoords->demarshall();
      tscope_->pointSubarray(*subarrayCoords);
      break;

   case TSCOPE_BF_SET_COORDS:
      beamCoords = static_cast<TscopeBeamCoords*> (bodybuff);
      beamCoords->demarshall();
      tscope_->beamformerSetCoords(*beamCoords);
      break;

   case TSCOPE_BF_SET_NULL_TYPE:
      nullType = static_cast<TscopeNullType*> (bodybuff);
      nullType->demarshall();
      tscope_->beamformerSetNullType(*nullType);
      break;

   case TSCOPE_BF_CLEAR_NULLS:
      tscope_->beamformerClearNulls();
      break;

   case TSCOPE_BF_ADD_NULL:
      beamCoords = static_cast<TscopeBeamCoords*> (bodybuff);
      beamCoords->demarshall();
      tscope_->beamformerAddNullCoords(*beamCoords);
      break;

   case TSCOPE_BF_POINT:
      tscope_->beamformerPoint();
      break;

   case TSCOPE_BF_CLEAR_COORDS:
      tscope_->beamformerClearBeamCoords();
      break;

   case TSCOPE_BF_CLEAR_ANTS:
      tscope_->beamformerClearAnts();
      break;

   case TSCOPE_REQUEST_POINT_CHECK:
      subarrayCoords = static_cast<TscopeSubarrayCoords*> (bodybuff);
      subarrayCoords->demarshall();
      tscope_->requestPointingCheck(*subarrayCoords);
      break;

   case TSCOPE_REQUEST_INTRINSICS:
      tscope_->requestIntrinsics();
      break;

   case TSCOPE_RESET:
      tscope_->reset();
      break;

   case TSCOPE_SHUTDOWN:
      tscope_->shutdown();
      break;

   case TSCOPE_REQUEST_STATUS:
      tscope_->requestStatus();
      break;

   case TSCOPE_STOP:
      stopReq = static_cast<TscopeStopRequest*> (bodybuff);
      stopReq->demarshall();
      tscope_->stop(*stopReq);
      break;

   case TSCOPE_STOW:
      stowReq = static_cast<TscopeStowRequest*> (bodybuff);
      stowReq->demarshall();
      tscope_->stow(*stowReq);
      break;

   case TSCOPE_TUNE:
      tuneReq = static_cast<TscopeTuneRequest*> (bodybuff);
      tuneReq->demarshall();
      tscope_->tune(*tuneReq);
      break;

   case TSCOPE_WRAP:
      wrapReq = static_cast<TscopeWrapRequest*> (bodybuff);
      wrapReq->demarshall();
      tscope_->wrap(*wrapReq);
      break;

   case TSCOPE_ZFOCUS:
      zfocusReq = static_cast<TscopeZfocusRequest*> (bodybuff);
      zfocusReq->demarshall();
      tscope_->zfocus(*zfocusReq);
      break;

   case TSCOPE_LNA_ON:
      lnaOnReq = static_cast<TscopeLnaOnRequest*> (bodybuff);
      lnaOnReq->demarshall();
      tscope_->lnaOn(*lnaOnReq);
      break;

   case TSCOPE_PAM_SET:
      pamSetReq = static_cast<TscopePamSetRequest*> (bodybuff);
      pamSetReq->demarshall();
      tscope_->pamSet(*pamSetReq);
      break;

   case TSCOPE_SEND_BACKEND_CMD:
      backendCmd = static_cast<TscopeBackendCmd*> (bodybuff);
      backendCmd->demarshall();
      tscope_->sendBackendCmd(*backendCmd);
      break;


   case TSCOPE_BEGIN_SENDING_COMMAND_SEQUENCE:
      tscope_->beginSendingCommandSequence();
      break;
     
   case TSCOPE_DONE_SENDING_COMMAND_SEQUENCE:
      tscope_->doneSendingCommandSequence();
      break;

   case TSCOPE_SIMULATE:
      tscope_->setSimulated(true);
      break;

   case TSCOPE_UNSIMULATE:
      tscope_->setSimulated(false);
      break;

   default: 
     
      sendMsgToSse(SEVERITY_ERROR, string("Tscope::SseProxy::handleSseMsg: ")
                   + string("unexpected message code received: ")
                   + SseUtil::intToStr(hdr->code));

      break;
   };  


}
