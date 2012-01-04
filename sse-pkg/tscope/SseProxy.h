/*******************************************************************************

 File:    SseProxy.h
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



#ifndef sseproxy_h
#define sseproxy_h

#include "sseTscopeInterface.h"
#include "NssProxy.h"

class Tscope;

class SseProxy : public NssProxy
{

 public:

  SseProxy(ACE_SOCK_STREAM &stream);
  void setTscope(Tscope* tscope);
  Tscope* getTscope();
  void setVerbose(int level);

  // outgoing messages to sse
  void sendErrorMsgToSse(NssMessageSeverity severity, const string& text);
  void sendMsgToSse(NssMessageSeverity severity, const string& text);

  void sendStatus(const TscopeStatusMultibeam& status);
  void sendIntrinsics(const TscopeIntrinsics& intrinsics);
  void stopComplete();
  void trackingOn();
  void trackingOff();
  void cancelTimer();
  void ready();

 private:

  string getName();

  void sendMessage(int messageCode, 
		   int activityId = NSS_NO_ACTIVITY_ID,
		   int dataLength = 0, const void *msgBody = 0);

  void handleIncomingMessage(SseInterfaceHeader *hdr, void *bodybuff);

  // Disable copy construction & assignment.
  // Don't define these.
  SseProxy(const SseProxy& rhs);
  SseProxy& operator=(const SseProxy& rhs);

  // data
  Tscope* tscope_;
  int     verboseLevel_;


};



#endif // sseproxy_h
