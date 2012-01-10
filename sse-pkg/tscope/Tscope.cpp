/*******************************************************************************

 File:    Tscope.cpp
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


#include <ace/Reactor.h>
#include "Tscope.h"
#include "TscopeEventHandler.h"
#include "SseProxy.h"
#include "Assert.h"
#include "SseUtil.h"
#include "SseAstro.h"
#include "SseMsg.h"
#include "SseTscopeMsg.h"
#include "Verbose.h"
#include "CmdPattern.h"
#include "Angle.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;


class HandleControlResponse : public CmdPattern
{
public:
   HandleControlResponse(Tscope& tscope) : tscope_(tscope)
   {}

   virtual void execute(const string & text)
   {
      tscope_.handleControlResponse(text);
   }
    
private:
   Tscope& tscope_;

};


class HandleMonitorResponse : public CmdPattern
{
public:
   HandleMonitorResponse(Tscope& tscope) : tscope_(tscope)
   {}

   virtual void execute(const string & text)
   {
      tscope_.handleMonitorResponse(text);
   }
    
private:
   Tscope& tscope_;

};

static void setBackendHostInStatus(TscopeStatusMultibeam &status, const string & host)
{
   SseUtil::strMaxCpy(status.ataBackendHost, host.c_str(), MAX_TEXT_STRING);
}


Tscope::Tscope(SseProxy& sseProxy, const string &antControlServerName,
	       int controlPort, int monitorPort)
   :
   sseProxy_(sseProxy),
   controlEventCallback_(0), 
   controlEventHandler_(0),
   monitorEventCallback_(0),
   monitorEventHandler_(0),
   verboseLevel_(0), 
   simulated_(false),
   rcvrSkyTuneReqMhz_(1), 
   tracking_(false),
   tuned_(false),
   notTrackingTargetCount_(0),
   commandSequenceState_(COMMAND_SEQUENCE_NOT_ACTIVE)
{
   sseProxy_.setTscope(this);
   SseUtil::strMaxCpy(intrinsics_.interfaceVersionNumber,
		      SSE_TSCOPE_INTERFACE_VERSION, MAX_TEXT_STRING);

   controlEventCallback_ = new HandleControlResponse(*this);
   controlEventHandler_ = new TscopeEventHandler("control", sseProxy_,
						 *controlEventCallback_,
						 antControlServerName,
						 controlPort);

   monitorEventCallback_ = new HandleMonitorResponse(*this);
   monitorEventHandler_ = new TscopeEventHandler("monitor", sseProxy_,
						 *monitorEventCallback_,
						 antControlServerName,
						 monitorPort);

   setBackendHostInStatus(statusMultibeam_, antControlServerName);
    
};

Tscope::~Tscope()
{
   VERBOSE2(verboseLevel_, "Tscope destructor" << endl;);

   delete controlEventCallback_;
   delete controlEventHandler_;
   delete monitorEventCallback_;
   delete monitorEventHandler_;

};

ostream& operator << (ostream &strm, Tscope& tscope)
{
   strm << "Telescope: " << endl
	<< "-----------" << endl
	<< "ant server: " << tscope.getControlHandler().getServerName() << endl
	<< "control port: " << tscope.getControlHandler().getPort() << endl
	<< "monitor port: " << tscope.getMonitorHandler().getPort() << endl
	<< "simulated: " << tscope.simulated_ << endl
	<< "verbose level: " << tscope.verboseLevel_ << endl;

   return strm;
}

TscopeIntrinsics Tscope::getIntrinsics()
{
   return(intrinsics_);
}

SseProxy & Tscope::getSseProxy()  
{
   return(sseProxy_);
}

TscopeEventHandler & Tscope::getControlHandler()
{
   return(*controlEventHandler_);
}

TscopeEventHandler & Tscope::getMonitorHandler()
{
   return(*monitorEventHandler_);
}

// Forward the command to the telescope.
// If there is an error in sending the command,
// send an error message back to the SSE.
// Returns true on success.

bool Tscope::sendCommand(const string & command)
{
   bool success = true;

   if (!getControlHandler().sendCommand(command))
   {
      stringstream errorStrm;

      errorStrm << "failure sending command " 
		<< " to telescope " << getName()
		<< ": " 
		<< command;   // already has a newline

      getSseProxy().sendErrorMsgToSse(
	 SEVERITY_ERROR, errorStrm.str());

      VERBOSE1(getVerboseLevel(), errorStrm.str() << endl;);

      success = false;
   }
    
   return success;
}

void Tscope::setVerboseLevel(int level)
{
   verboseLevel_ = level;
   getControlHandler().setVerboseLevel(level);
   getMonitorHandler().setVerboseLevel(level);
   getSseProxy().setVerbose(level);
}


int Tscope::getVerboseLevel()
{
   return verboseLevel_;
}


void Tscope::setName(const string &name)
{
   SseUtil::strMaxCpy(intrinsics_.name, name.c_str(), MAX_TEXT_STRING);
}

string Tscope::getName() 
{
   return(intrinsics_.name);
}

TscopeStatusMultibeam Tscope::getStatusMultibeam() const
{
   return(statusMultibeam_);
}

bool Tscope::getSimulated() 
{
   return(simulated_);
}

void Tscope::setSimulated(bool simulated)
{
   VERBOSE1(getVerboseLevel(), "Tscope simulate mode: "
	    << simulated << endl;);

   simulated_ = simulated;

   statusMultibeam_.simulated = SSE_FALSE;
   if (simulated_)
   {
      statusMultibeam_.simulated = SSE_TRUE;
   }

   requestStatus();
}


void Tscope::connect()
{
   // make sure simulation mode is off to avoid 
   // any interactions with ant server simulator
   setSimulated(false);

   getControlHandler().connect();
   getMonitorHandler().connect();
}

void Tscope::disconnect()
{
   getControlHandler().disconnect();
   getMonitorHandler().disconnect();

   // indicate via the status that the connection has been broken:
   TscopeStatusMultibeam defaultStatus;
   statusMultibeam_ = defaultStatus;

   // send status to SSE
   requestStatus();
}

void Tscope::setAntControlServerName(const string &serverName)
{
   getControlHandler().setServerName(serverName);
   getMonitorHandler().setServerName(serverName);

   setBackendHostInStatus(statusMultibeam_, serverName);
}



// ----- incoming messages from SSE Proxy ------

// TBD add to proxy protocol

static void setCurrentTimeInStatus(TscopeStatusMultibeam &status)
{
   SseUtil::strMaxCpy(status.time, SseUtil::currentIsoDateTime().c_str(),
		      MAX_TEXT_STRING);
}

void Tscope::handleCommand(const string & command)
{
   if (!getSimulated())
   {
      sendCommand(command);
   }
}


void Tscope::handleDeferredBfCommand(const string & command)
{
   if (commandSequenceState_ == COMMAND_SEQUENCE_INCOMING)
   {
      /*
        Remember this command, execute it later, when 
        the subarrays are all on target.  A "READY" response
        is expected to come back when the command completes.
      */
      deferredBeamformerCmd_ = command;
   }
   else
   {
      // Do it now
      handleCommand(command);
   }
}


void Tscope::allocate(const TscopeSubarray & subarray) 
{
   stringstream command;
   command << "ALLOCATE " << subarray.subarray << endl;
   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);

   handleCommand(command.str());
}

void Tscope::deallocate(const TscopeSubarray & subarray)
{
   stringstream command;
   command << "DEALLOCATE " << subarray.subarray << endl;
   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);
   
   handleCommand(command.str());
}

// assign subarray of ants to synth beam
void Tscope::assignSubarray(const TscopeAssignSubarray & assignSub)
{
   stringstream command;

   if (assignSub.beam <= TSCOPE_INVALID_BEAM ||
       assignSub.beam >= TSCOPE_N_BEAMS)
   {
      reportErrorToSse("assignsubarray: invalid beam specified");
      return;
   }

   command << "BF SET ANTS " 
           << SseUtil::strToUpper(SseTscopeMsg::beamToName(assignSub.beam))
           << " "
           << assignSub.subarray 
           << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);
   if (!getSimulated())
   {
      if (!sendCommand(command.str()))
      {
	 return;
      }
   }

   ataAssignSubarray_.push_back(assignSub);

}

void Tscope::antgroupAutoselect(const TscopeAntgroupAutoselect &antAuto)
{
   stringstream command;

   command << "ANTGROUP AUTOSELECT"
           << " BFLIST " << antAuto.bflist
           << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);
   
   //handleCommand(command.str());
//JR - uncomment this to do bfantselect
   deferredBeamformerCmd_.erase();
   handleDeferredBfCommand(command.str());
}

void Tscope::beamformerStop()
{
   // In case a deferred command is pending
   deferredBeamformerCmd_.erase();

   string command("BF STOP\n");
   VERBOSE1(getVerboseLevel(), "Tscope command: " << command << endl;);

   handleCommand(command);
}

void Tscope::beamformerReset()
{
   string command("BF RESET\n");
   VERBOSE1(getVerboseLevel(), "Tscope command: " << command << endl;);

   handleDeferredBfCommand(command);
}


void Tscope::beamformerInit()
{
   string command("BF INIT\n");
   VERBOSE1(getVerboseLevel(), "Tscope command: " << command << endl;);

   handleDeferredBfCommand(command);
}

//JR - Added to tell bfinit the destination IP addresses
void Tscope::beamformerDest(TscopeBackendCmd args)
{
   stringstream cmd;
   cmd << args.cmdWithArgs << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());

}

void Tscope::beamformerAutoatten()
{
   string cmd("BF AUTOATTEN\n");
   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd << endl;);

   handleDeferredBfCommand(cmd);
}


// set period of periodic monitor reports 
void Tscope::monitor(const TscopeMonitorRequest & request)
{
   stringstream command;
   command << "MONITOR " << request.periodSecs << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);
   handleCommand(command.str());
}


/*
  Beginning a new commanding sequence.
  Forget old, and remember new:
     Synth beam to subarray associations.
     Synth beam pointing requests
     Subarray pointing requests.
 */
void Tscope::beginSendingCommandSequence()
{ 
   VERBOSE2(getVerboseLevel(), 
            "Tscope command: beginSendingCommandSequence" << endl;);

   commandSequenceState_ = COMMAND_SEQUENCE_INCOMING;

   tracking_ = false;
   tuned_ = false;

   tuneRequests_.clear();
   subarrayPointingRequests_.clear();
   ataAssignSubarray_.clear();

   deferredBeamformerCmd_.erase();
}

/*
  End of new commanding sequence.
  OK to tell SSE that we're ready
  as soon as the tscope status indicates
  that it's so.
 */
void Tscope::doneSendingCommandSequence()
{
   VERBOSE2(getVerboseLevel(),
            "Tscope command: doneSendingCommandSequence " << endl;);

   commandSequenceState_ = COMMAND_SEQUENCE_FULLY_RECEIVED;

   // TBD fix simulator mode to handle multiple target requests
   if (getSimulated())
   { 
      // simulate a little slew time
      const int simulatedSlewTimeSecs(2);
      sleep(simulatedSlewTimeSecs); 
      getSseProxy().ready();
      commandSequenceState_ = COMMAND_SEQUENCE_NOT_ACTIVE;
   }

}

/*
  Return the first synth beam name associated with the
  subarray. Return TSCOPE_INVALID_BEAM if not found.
 */
TscopeBeam Tscope::getBeamIndexForSubarray(const string &subarray)
{
   for (vector<TscopeAssignSubarray>::iterator it = ataAssignSubarray_.begin();
        it != ataAssignSubarray_.end(); ++it)
   {
      const TscopeAssignSubarray & assignSub(*it);
      if (subarray == assignSub.subarray)
      {
         // TBD change from assert to sending an error
         Assert(assignSub.beam > TSCOPE_INVALID_BEAM && 
                assignSub.beam < TSCOPE_N_BEAMS);
         return assignSub.beam;
      }
   }

   return TSCOPE_INVALID_BEAM;
}

bool Tscope::haveRepeatedPointingRequest(
   const TscopeSubarrayCoords & coords)
{
   // Check for repeated pointing requests for subarray
   bool repeatedRequests = false;
   for (unsigned int i=0; i<subarrayPointingRequests_.size(); ++i)
   {
      bool subarraysMatch = (
         string(coords.subarray) == 
         string(subarrayPointingRequests_[i].subarray));
      
      if (subarraysMatch)
      {
         stringstream strm;
         strm << "Tscope: multiple pointing requests for same "
              << "subarray:\n" 
              << "older request:\n" << subarrayPointingRequests_[i] << "\n"
              << "latest request:\n" << coords;
         getSseProxy().sendErrorMsgToSse(SEVERITY_ERROR, strm.str());
         
         repeatedRequests = true;
         break;
      }
   }
 
   return repeatedRequests;
   
}


/*
  Don't point this target request, just remember it for
  on-target checking.
 */
void Tscope::requestPointingCheck(const TscopeSubarrayCoords & coords)
{
   if (commandSequenceState_ == COMMAND_SEQUENCE_INCOMING)
   {
       if (haveRepeatedPointingRequest(coords))
       {
          return;
       }

      // remember pointing so on-target tracking can be determined
      subarrayPointingRequests_.push_back(coords);
   }
}

void Tscope::beamformerSetCoords(const TscopeBeamCoords & coords)
{
   stringstream command;

   command.precision(6);  // N digits after the decimal
   command.setf(std::ios::fixed);  // show all decimal places up to precision

   command << "BF SET COORDS ";

   if (coords.beam <= TSCOPE_INVALID_BEAM ||
       coords.beam >= TSCOPE_N_BEAMS)
   {
      reportErrorToSse("setBeamCoords: invalid beam specified");
      return;
   }
   
   command << SseUtil::strToUpper(SseTscopeMsg::beamToName(coords.beam))
           << " ";

   switch(coords.pointing.coordSys)
   {

   case TscopePointing::AZEL:

      command << "AZEL " << coords.pointing.azDeg << " " << coords.pointing.elDeg << endl;

      // set the status so that it includes this pointing information
      if (getSimulated())
      {
	 setCurrentTimeInStatus(statusMultibeam_);
         statusMultibeam_.synthPointing[coords.beam].azDeg = coords.pointing.azDeg;
         statusMultibeam_.synthPointing[coords.beam].elDeg = coords.pointing.elDeg;
	 getSseProxy().sendStatus(statusMultibeam_);   
      }

      break;

   case TscopePointing::J2000:

      command << "J2000 " << coords.pointing.raHours << " " << coords.pointing.decDeg << endl;

      // set the status so that it includes this pointing information
      if (getSimulated())
      {
	 setCurrentTimeInStatus(statusMultibeam_);
         statusMultibeam_.synthPointing[coords.beam].raHours = coords.pointing.raHours;
         statusMultibeam_.synthPointing[coords.beam].decDeg = coords.pointing.decDeg;
	 getSseProxy().sendStatus(statusMultibeam_);   
      }

      break;

   case TscopePointing::GAL:
      
      command << "GAL " << coords.pointing.galLongDeg << " " 
	      << coords.pointing.galLatDeg << endl;
      
      // set the status so that it includes this pointing information
      if (getSimulated())
      {
	 setCurrentTimeInStatus(statusMultibeam_);
         statusMultibeam_.synthPointing[coords.beam].galLongDeg = coords.pointing.galLongDeg;
         statusMultibeam_.synthPointing[coords.beam].galLatDeg = coords.pointing.galLatDeg;
	 getSseProxy().sendStatus(statusMultibeam_);   
      }

      break;

   default:

      stringstream strm;
      strm << "Tscope unsupported coordinate system: " 
	   << coords.pointing.coordSys;
      getSseProxy().sendErrorMsgToSse(SEVERITY_ERROR, strm.str());
      return;

   }

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);
   VERBOSE2(getVerboseLevel(), "Coords: " << coords << endl;);

   handleCommand(command.str());
}

void Tscope::beamformerAddNullCoords(const TscopeBeamCoords & coords)
{
   stringstream command;

   command.precision(6);  // N digits after the decimal
   command.setf(std::ios::fixed);  // show all decimal places up to precision

   command << "BF ADD NULL ";

   if (coords.beam <= TSCOPE_INVALID_BEAM ||
       coords.beam >= TSCOPE_N_BEAMS)
   {
      reportErrorToSse("addNullCoords: invalid beam specified");
      return;
   }
   
   command << SseUtil::strToUpper(SseTscopeMsg::beamToName(coords.beam))
           << " ";

   switch(coords.pointing.coordSys)
   {

   case TscopePointing::AZEL:

      command << "AZEL " << coords.pointing.azDeg << " " 
              << coords.pointing.elDeg << endl;
      break;

   case TscopePointing::J2000:

      command << "J2000 " << coords.pointing.raHours 
              << " " << coords.pointing.decDeg << endl;
      break;

   case TscopePointing::GAL:
      
      command << "GAL " << coords.pointing.galLongDeg << " " 
	      << coords.pointing.galLatDeg << endl;
      break;

   default:

      stringstream strm;
      strm << "Tscope unsupported coordinate system: " 
	   << coords.pointing.coordSys;
      getSseProxy().sendErrorMsgToSse(SEVERITY_ERROR, strm.str());
      return;

   }

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);
   VERBOSE2(getVerboseLevel(), "Coords: " << coords << endl;);

   handleCommand(command.str());
}


void Tscope::beamformerSetNullType(const TscopeNullType& nullType)
{
   stringstream command;

   command << "BF SET NULLTYPE ";
   command << SseTscopeMsg::nullTypeToString(nullType.nullType);
   command << "\n";

   handleCommand(command.str());
}

void Tscope::beamformerClearNulls()
{
   stringstream command;

   command << "BF CLEAR NULLS ALL";
   command << "\n";

   handleCommand(command.str());
}


/*
  Tell beamformer to point all beams previously assigned coordinates.
 */
void Tscope::beamformerPoint()
{
   stringstream cmd;
   cmd << "BF POINT" << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);

   handleDeferredBfCommand(cmd.str());
}


void Tscope::beamformerCal(const TscopeCalRequest &cal)
{
   stringstream command;
   command.precision(6);  // N digits after the decimal
   command.setf(std::ios::fixed);  // show all decimal places up to precision

   command << "BF CAL ";

   switch(cal.calType)
   {
   case TscopeCalRequest::DELAY:
      command << "DELAY";
      break;

   case TscopeCalRequest::PHASE:
      command << "PHASE";
      break;

   case TscopeCalRequest::FREQ:
      command << "FREQ";
      break;

   default:
      stringstream strm;
      strm << "Tscope unsupported cal type: " 
	   << cal.calType;
      getSseProxy().sendErrorMsgToSse(SEVERITY_ERROR, strm.str());

      return;
   }

   command << " INTEGRATE " << cal.integrateSecs
           << " CYCLES " << cal.numCycles;

   command << " ITERATE " << cal.calIterations;

   command << "\n";

   VERBOSE1(getVerboseLevel(), "Tscope command: " 
            << command.str() << endl;); 

   handleDeferredBfCommand(command.str());
}



// clear all coordinates assigned to beams
void Tscope::beamformerClearBeamCoords()
{
   stringstream cmd;
   cmd << "BF CLEAR COORDS ALL" << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());
}

void Tscope::beamformerClearAnts()
{
   stringstream cmd;
   cmd << "BF CLEAR ANTS ALL" << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());
}


void Tscope::pointSubarray(const TscopeSubarrayCoords & coords)
{
   stringstream command;

   command.precision(6);  // N digits after the decimal
   command.setf(std::ios::fixed);  // show all decimal places up to precision

   command << "POINT " << coords.subarray
           << " ";

   // TBD in sim mode, update subarray pointing status.

   switch(coords.pointing.coordSys)
   {

   case TscopePointing::AZEL:

      command << "AZEL " << coords.pointing.azDeg << " " << coords.pointing.elDeg << endl;

      break;

   case TscopePointing::J2000:

      command << "J2000 " << coords.pointing.raHours << " " << coords.pointing.decDeg << endl;

      break;

   case TscopePointing::GAL:

      command << "GAL " << coords.pointing.galLongDeg << " " 
	      << coords.pointing.galLatDeg << endl;
      break;

   default:

      stringstream strm;
      strm << "Tscope unsupported coordinate system: " 
	   << coords.pointing.coordSys;
      getSseProxy().sendErrorMsgToSse(SEVERITY_ERROR, strm.str());
      return;

   }

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);
   VERBOSE2(getVerboseLevel(), "Coords: " << coords << endl;);
 
   if (!getSimulated()) 
   {
      if (! sendCommand(command.str()))
      {
	 return;
      }
   }

   if (commandSequenceState_ == COMMAND_SEQUENCE_INCOMING)
   {
      if (haveRepeatedPointingRequest(coords))
      {
         return;
      }
      
      // remember pointing so on-target tracking can be determined
      subarrayPointingRequests_.push_back(coords);
   }

}

void Tscope::requestStatus()
{
   VERBOSE1(getVerboseLevel(), "TScope request status." << endl;);

   // TBD this does not request an updated status,
   // but sends the cached one which is presumably being updated
   // periodically via the monitor port.

   if (getSimulated())
   {
      setCurrentTimeInStatus(statusMultibeam_);
   }

   getSseProxy().sendStatus(statusMultibeam_);   

}

void Tscope::requestIntrinsics() 
{
   VERBOSE1(getVerboseLevel(), "TScope request intrinsics." << endl;);
   getSseProxy().sendIntrinsics(intrinsics_);   
}

void Tscope::reset()
{
   VERBOSE1(getVerboseLevel(), "TScope reset." << endl;);

   // TBD

}

void Tscope::tune(const TscopeTuneRequest & tuneReq)
{
   stringstream command;

   // print rfSkyFreq to 0.01 Hz
   const int precision = 8; 

   command.precision(precision);  // N digits after the decimal
   command.setf(std::ios::fixed);  // show all decimal places up to precision

   if(tuneReq.tuning <= TSCOPE_INVALID_TUNING ||
      tuneReq.tuning >= TSCOPE_N_TUNINGS)
   {
      reportErrorToSse("tune: invalid tuning name specified");
      return;
   }
   
   // Make sure tune freq is above zero, so that beamsize calculation
   // doesn't fail
   if (tuneReq.skyFreqMhz < 1.0)
   {
      reportErrorToSse("tune: invalid frequency given");
      return;
   }

   // extract last letter of full tuning name: TUNINGx
   string tuningFullName(SseTscopeMsg::tuningToName(tuneReq.tuning));
   Assert(tuningFullName.length() > 0);
   char tuningLetter(tuningFullName.at(tuningFullName.length()-1));

   // command format: TUNE {A-D} freqMHz
   command << "TUNE " << tuningLetter << " " << tuneReq.skyFreqMhz << "\n";

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);

   if (getSimulated())
   {
      statusMultibeam_.tuning[tuneReq.tuning].skyFreqMhz = tuneReq.skyFreqMhz;
      requestStatus();
   }
   else  // not simulated
   {
      if (!sendCommand(command.str()))
      {
	 return;
      }
   }
  
   if (commandSequenceState_ ==  COMMAND_SEQUENCE_INCOMING)
   {
      // remember tuning so it can be compared against status for 'ready' condition
      tuneRequests_.push_back(tuneReq);
   }

   /*
     Latest tune request is remembered for use in determining
     beam size.  TBD: what to do if the tunings have frequencies
     that are widely different.
    */
   
   rcvrSkyTuneReqMhz_ = tuneReq.skyFreqMhz;

}

void Tscope::zfocus(const TscopeZfocusRequest & zfocusReq)
{
   stringstream command;

   // print rfSkyFreq to 0.01 Hz
   const int precision = 8; 

   command.precision(precision);  // N digits after the decimal
   command.setf(std::ios::fixed);  // show all decimal places up to precision

   command << "ZFOCUS " 
           << zfocusReq.subarray << " "
	   << zfocusReq.skyFreqMhz << "\n";

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);

   if (getSimulated())
   {

#if 0
// TBD fix for new ata interface
      statusMultibeam_.zfocusMhz = zfocusReq.skyFreqMhz;
#endif
      requestStatus();
   }
   else  // not simulated
   {
      if (!sendCommand(command.str()))
      {
	 return;
      }
   }
  
}

void Tscope::lnaOn(const TscopeLnaOnRequest & lnaOnReq)
{
   stringstream cmd;
   cmd << "LNAON " << lnaOnReq.subarray << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());
}

void Tscope::pamSet(const TscopePamSetRequest & pamSetReq)
{
   stringstream cmd;
   cmd << "PAMSET " << pamSetReq.subarray << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());
}

void Tscope::sendBackendCmd(const TscopeBackendCmd & backendCmd)
{
   stringstream cmd;
   cmd << backendCmd.cmdWithArgs << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());
}

void Tscope::shutdown()
{
   VERBOSE1(getVerboseLevel(), "Tscope: " << getName() << " shutdown." << endl;);
   ACE_Reactor::end_event_loop ();
}

void Tscope::stop(const TscopeStopRequest & stopReq)
{
   stringstream cmd;
   cmd << "STOP " << stopReq.subarray << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());
}

void Tscope::stow(const TscopeStowRequest & stowReq)
{
   stringstream cmd;
   cmd << "STOW " << stowReq.subarray << endl;

   VERBOSE1(getVerboseLevel(), "Tscope command: " << cmd.str() << endl;);
   handleCommand(cmd.str());
}

void Tscope::wrap(TscopeWrapRequest wrapReq)
{
   stringstream command;

   command << "WRAP " << wrapReq.subarray << " "
           << wrapReq.wrapNumber << "\n";

   VERBOSE1(getVerboseLevel(), "Tscope command: " << command.str() << endl;);

   if (getSimulated())
   {
#if 0
// TBD fix for new ata interface
      statusMultibeam_.wrap = wrapReq.wrapNumber;
#endif
      requestStatus();
   }
   else // not simulated
   {
      if (! sendCommand(command.str()))
      {
	 return;
      }
   }
}


// compute beamsize for currently requested rcvr sky freq
double Tscope::beamsizeDeg() const
{
   Assert(rcvrSkyTuneReqMhz_ > 0);
   const double MhzPerGhz = 1000;
   double rcvrSkyFreqGhz = rcvrSkyTuneReqMhz_ / MhzPerGhz;

   const double AtaFovAtOneGHzDeg = 3.5;    // TBD get from parameters?
   double beamsizeDeg = AtaFovAtOneGHzDeg / rcvrSkyFreqGhz;

   return beamsizeDeg;
}


// TBD what is the maxPointingError for multiple RF tunings?
double Tscope::maxPointingErrorDeg() const
{
   // Max pointing error for ATA is 1/10 beamwidth
   const double PointingErrorBeamFraction = 0.1;
   double maxPointingErrorDeg = beamsizeDeg() * PointingErrorBeamFraction;

   // TBD
   // Temp fix to account for refraction error in ATA Ra/Dec coords
   double minErrorCutoffDeg(0.1);
   maxPointingErrorDeg = max(maxPointingErrorDeg, minErrorCutoffDeg);

   return maxPointingErrorDeg;
}

// compute the absolute difference between two degree values

static double absDiffDeg(double value1Deg, double value2Deg)
{
   double diffDeg = fabs(value1Deg - value2Deg);

#if 0
   // TBD:  how does the telescope behave when the
   // coordinates wrap around?

   // Handle circle wraparound (i.e., find the shortest
   // way around)
   const double DegPerCircle = 360;
   const double DegPerHalfCircle = 360 / 2;
   if (diffDeg > DegPerHalfCircle)
   {
      diffDeg = DegPerCircle - diffDeg;
   }
#endif

   return diffDeg;
}

// arc distance separation in degrees between two ra/dec coords
static double arcDistDeg(
   double firstRaHours, double firstDecDeg,
   double secondRaHours, double secondDecDeg)
{
   double firstRaRads(SseAstro::hoursToRadians(firstRaHours));
   double firstDecRads(SseAstro::degreesToRadians(firstDecDeg));

   double secondRaRads(SseAstro::hoursToRadians(secondRaHours));
   double secondDecRads(SseAstro::degreesToRadians(secondDecDeg));

   double distRads(SseAstro::angSepRads(firstRaRads, firstDecRads,
                                        secondRaRads, secondDecRads));

   double distDeg(SseAstro::radiansToDegrees(distRads));

#if 0
// debug  
   cout << "arcDistDeg: " << endl
        << "first ra/dec: " << firstRaHours << " " << firstDecDeg << endl
        << "second ra/dec: " << secondRaHours << " " << secondDecDeg << endl
        << "distDeg:" << distDeg << endl;
#endif

   return distDeg;
}



// TBD: change this to do a real great circle calculation
// for az/el & gal coords.

bool Tscope::isRequestedTargetPositionCloseToCurrentPosition(
   const TscopePointing& requested, TscopePointing & current)
{
   // If the difference between the requested position and the
   // current position is within the tolerance, then return true.

   const double tolDeg(maxPointingErrorDeg());

   // Convert the requested & current coordinates to degrees as needed.
   // Use the status fields that match the requested coord sys
   // for comparison.  If the requested and current coordinate systems
   // don't match, then return false;

   if (requested.coordSys != current.coordSys)
   {
      return false;
   }

   double requestedCoord1Deg(0);
   double requestedCoord2Deg(0);
   double currentCoord1Deg(0);
   double currentCoord2Deg(0);

   double requestedToCurrentArcDistDeg(0);

   switch (requested.coordSys)
   {
   case TscopePointing::AZEL:

      // All coords are already in degrees, no conversions necessary
      requestedCoord1Deg = requested.azDeg;
      requestedCoord2Deg = requested.elDeg;
	
      currentCoord1Deg = current.azDeg;
      currentCoord2Deg = current.elDeg;

      break;
	
   case TscopePointing::J2000:

      requestedToCurrentArcDistDeg = arcDistDeg(
         requested.raHours, requested.decDeg,
         current.raHours, current.decDeg);

      if (requestedToCurrentArcDistDeg < tolDeg)
      {
         return true;
      }
      else
      {
#if 0
        // debug
        string status = "J2000 pointing error: " + doubleToString(requestedToCurrentArcDistDeg) + " degrees Arc diff. Requested RA=" + doubleToString(requested.raHours) + ", DEC=" + doubleToString(requested.decDeg) + ", Actual RA=" + doubleToString(current.raHours) + ", DEC=" +  doubleToString(current.decDeg) + ", tol=" + doubleToString(tolDeg) ;
        printPointingRequestsAndStatus("debug of 'lost target tracking, invalid pointing': " + status);
#endif
        return false;
      }

      break;
	
   case TscopePointing::GAL:

      // All coords are already in degrees, no conversions necessary
      requestedCoord1Deg = requested.galLongDeg;
      requestedCoord2Deg = requested.galLatDeg;
	
      currentCoord1Deg = current.galLongDeg;
      currentCoord2Deg = current.galLatDeg;

      break;
	
   default:

      reportErrorToSse("received invalid target coordinate type");
      break;
	
   }

   if (absDiffDeg(requestedCoord1Deg, currentCoord1Deg) <= tolDeg 
       && absDiffDeg(requestedCoord2Deg, currentCoord2Deg) <= tolDeg)
   {
      return true;
   }

#if 0

        string status = "Lost tracking ERROR!, Requested Coord1=" + doubleToString(requestedCoord1Deg) + ", current 1= " + doubleToString(currentCoord1Deg) + ", Requested Coord2=" + doubleToString(requestedCoord2Deg) + ", current 2= " + doubleToString(currentCoord2Deg) + ", tol=" + doubleToString(tolDeg);
        printPointingRequestsAndStatus("debug of 'lost target tracking, invalid pointing': " + status);
#endif
   return false;

}


/*
  Process multiline response message.
  Most lines of interest will start with one of these keywords:
      OK, ERROR, WARNING, INFO, READY
 */
void Tscope::handleControlResponse(const string & totalMsg)
{
   // ignore message if it's all blank text
   if (totalMsg.find_first_not_of("\n ") != string::npos)
   {
      VERBOSE1(getVerboseLevel(), "Tscope::handleControlResponse: " 
	       << "received command response: " << totalMsg << endl;);

      // split message into lines, process each
      vector<string> lines(SseUtil::tokenize(totalMsg, "\n"));
      
      for (unsigned int i=0; i<lines.size(); ++i)
      {
         string uppercaseMsg(SseUtil::strToUpper(lines[i]));
         
         if (uppercaseMsg.find("OK") == 0)
         {
            // ignore it
         }
         else if (uppercaseMsg.find("ERROR") == 0)
         {
            /*** temp bypass ***/
            
            /*** ignore "Cannot set frequency for tuning" errors
                 which are not reliable ****/
            //JR - Nov 24, 2010 - The tune frequency command is very reliable
            //so I am removing this bypass. We had a problem where the tunings
            //could not be set, and the tasks continued on anyway, which is
            //not desireable. This was redmine issue setiQuest->SonATA - 120
/*
            if (lines[i].find("Cannot set frequency for tuning") != string::npos)
            {
               // write to stdout so it will at least go to components log
               cout << lines[i] << endl;
            }
            else
*/
            {
               getSseProxy().sendErrorMsgToSse(SEVERITY_ERROR, lines[i]);
            }
         }
         else if (uppercaseMsg.find("WARNING") == 0)
         {
            getSseProxy().sendMsgToSse(SEVERITY_WARNING, lines[i]);
         }
         else if (uppercaseMsg.find("INFO") == 0)
         {
            getSseProxy().sendMsgToSse(SEVERITY_INFO, lines[i]);
         }
         else if (uppercaseMsg.find("READY") == 0)
         {
            sseProxy_.ready();
         }
         else 
         {
            // Send any line that remains as info
            getSseProxy().sendMsgToSse(SEVERITY_INFO, lines[i]);
         }
      }
   }
}


void Tscope::handleMonitorResponse(const string & text)
{
   parseStatus(text);
}

void Tscope::reportStatusToSse()
{
   sseProxy_.sendStatus(statusMultibeam_); 

   // Don't check status while command sequence is being updated
   if (commandSequenceState_ != COMMAND_SEQUENCE_INCOMING)
   {
      reportTargetTracking();
      reportTunings();
   }

   // Verify that status matches all the commands in the command sequence.
   if (commandSequenceState_ == COMMAND_SEQUENCE_FULLY_RECEIVED)
   {
      bool tuningsReady(true);
      if (! tuneRequests_.empty())
      {
         tuningsReady = tuned_;
      }

      bool pointingsReady(true);
      if (! subarrayPointingRequests_.empty())
      {
         pointingsReady = tracking_;
      }

      VERBOSE2(getVerboseLevel(), "reportStatusToSse():\n"
               << "#tuningRequests=" << tuneRequests_.size()
               << " #subarrayPointingRequests=" << subarrayPointingRequests_.size()
               << endl;);

      VERBOSE2(getVerboseLevel(), "reportStatusToSse():\n"
               << "tuningsReady=" << tuningsReady
               << " pointingsReady=" << pointingsReady
               << endl;);

      if (tuningsReady && pointingsReady)
      {

         if (deferredBeamformerCmd_.empty())
         {
            /*
              No beamformer commands (tuning only)
              so issue a "Ready" right away.
            */
            sseProxy_.ready();
         }
         else
         {
	sseProxy_.cancelTimer();
            getSseProxy().sendMsgToSse(
               SEVERITY_INFO, 
               "Primary beam is ready.  Now running beamformer cmd: "
               + deferredBeamformerCmd_);

            // Subarrays are on target, so issue final beamformer command
            handleCommand(deferredBeamformerCmd_);
         }

         commandSequenceState_ = COMMAND_SEQUENCE_NOT_ACTIVE;
      }
   }
}

// Verify that status matches all the tunings in the command sequence.
void Tscope::reportTunings()
{
   bool allGoodTunings(true);
   string errorText = "Tuning changed unexpectedly:";

   // ATA currently reports nearest KHz. 
   // This tolerance setting should probably be configurable.
   double tuningTolMHz(0.002);  

   for(vector<TscopeTuneRequest>::iterator tuneReq = tuneRequests_.begin();
       tuneReq != tuneRequests_.end(); ++tuneReq)
   {
      const TscopeTuneRequest & request(*tuneReq);

      VERBOSE2(getVerboseLevel(), "verifyTunings():\n"
               << "request: " << request << endl;);

      TscopeTuning tuningIndex = request.tuning;
      if (tuningIndex <= TSCOPE_INVALID_TUNING || 
          tuningIndex >= TSCOPE_N_TUNINGS)
      {
         reportErrorToSse("invalid tuning specified in tuning request");
         allGoodTunings = false;
         break;
      }

      VERBOSE2(getVerboseLevel(), "verifyTunings():\n"
               << " tuning: " << SseTscopeMsg::tuningToName(tuningIndex)
               << " req skyfreq: "
               << request.skyFreqMhz
               << " status skyfreq: " 
               << statusMultibeam_.tuning[tuningIndex].skyFreqMhz
               << endl;);

      if (fabs(statusMultibeam_.tuning[tuningIndex].skyFreqMhz - 
               request.skyFreqMhz) > tuningTolMHz) 
      {
         stringstream errorStrm;
         errorStrm << " tuning: " << SseTscopeMsg::tuningToName(tuningIndex)
                   << ", requested skyfreq: "
                   << request.skyFreqMhz
                   << " MHz, status skyfreq: " 
                   << statusMultibeam_.tuning[tuningIndex].skyFreqMhz
                   << " MHz";
         errorText += errorStrm.str();
            
         // tuning does not match
         allGoodTunings = false;
         break;
      }
   }


/****** temp bypass  *****/
/*** 
assume all tunings are ok until status values are reliable.
****/

   allGoodTunings = true;

/**** bypass ***/

   if (!tuned_ && allGoodTunings)
   {
      tuned_ = true;
   }
   else if (tuned_ && !allGoodTunings)
   {
      tuned_ = false;
      reportErrorToSse(errorText);
   }

}

void Tscope::reportTargetTracking()
{
   /*
     Notify the sse when there's a change in target tracking;
     ie, started tracking the target, or lost tracking of the target.

     The target is being tracked when these conditions are met:
     1) the drive status is 'TRACK' (tracking)
     2) the status coord-sys is the same as the requested one and 
     the status position is close to the requested target position
     3) the great-circle tracking error is small enough

     Note: 2) is there primarily as a gross check to prevent premature
     'on-target' status when switching targets and the status has not 
     yet caught up to the new target request.

     We may want to rename this condition from "tracking" to something
     like "on target".

     When tracking the target, the tracking is allowed to go off
     maxNotTrackingTargetCount times before reporting that tracking
     was really lost, so that momentary losses of tracking do not 
     cause a premature declaration of this condition.

    */

   VERBOSE2(getVerboseLevel(), "reportTargetTracking():\n"
	    "maxPointingErrDeg = " << maxPointingErrorDeg() << endl;);

   const int maxNotTrackingTargetCount = 10;
   string GoodTrackStateKeyword("TRACK");
   string DriveErrorKeyword("DRIVE_ERROR");

   /* 
      Check the status of all requested pointings.
    */
   bool allGoodTrackState(true);
   bool allGoodPosition(true);
   bool allGoodGcError(true);

   // Assumes there is only 1 primary beam request.
   // TBD error if not.
   // Also assumes that all synth beam ant lists 
   // are a subset of the primary list.  TBD check for this?

   for(vector<TscopeSubarrayCoords>::iterator requestIt =
          subarrayPointingRequests_.begin();
       requestIt != subarrayPointingRequests_.end(); ++requestIt)
   {
      const TscopeSubarrayCoords & request(*requestIt);
      TscopeBeam beamIndex(TSCOPE_INVALID_BEAM);
      TscopePointing * primaryPointingStatus(0);
      /*
        Check the appropriate status associated with each
        subarray pointing request.
       */
      VERBOSE2(getVerboseLevel(), "reportTargetTracking():\n"
	    "checking pointing request: " << request << endl;);

      /* check the primary beam status for all assigned synth beams */
      for (vector<TscopeAssignSubarray>::iterator synthIt = 
              ataAssignSubarray_.begin();
           synthIt != ataAssignSubarray_.end(); ++synthIt)
      {
         const TscopeAssignSubarray & assignSub(*synthIt);
         beamIndex = assignSub.beam;

         primaryPointingStatus = &statusMultibeam_.primaryPointing[beamIndex];
         
         double minDishSuccessPercent(0.90);  // TBD make configurable
         bool goodTrackState(false);
         
         int totalDishes = statusMultibeam_.subarray[beamIndex].numTotal;
         if (totalDishes <= 0)
         {
            // TBD error 
            //reportErrorToSse("no dishes assigned to beam" + );
         }
         else 
         {
            /*
              Verify that the minimum required number of dishes
              are tracking, and that enough of them have been used
              to make up the primary beam pointing.
            */
            
            int minDishes = max(
               static_cast<int>(totalDishes * minDishSuccessPercent), 1);
            
            bool enoughTracking(statusMultibeam_.subarray[beamIndex].numTrack >=
                                minDishes);
            
            bool enoughInPrimary(
               statusMultibeam_.subarray[beamIndex].numSharedPointing >= 
               minDishes);
            
            if (enoughTracking && enoughInPrimary)
            {
               goodTrackState = true;
            }
            else if(goodTrackState && tracking_)
            {
              if(!enoughTracking)
                reportErrorToSse("tracking but not enough. numTrack=" +
                  intToString(statusMultibeam_.subarray[beamIndex].numTrack) + ", min dishes=" + 
                  intToString(minDishes));
              if(!enoughInPrimary)
                reportErrorToSse("tracking but not enough in primary. num=" +
                  intToString(statusMultibeam_.subarray[beamIndex].numSharedPointing) + 
                  ", num dishes=" + intToString(minDishes));
            }
         }

         bool goodPosition(isRequestedTargetPositionCloseToCurrentPosition(
            request.pointing, *primaryPointingStatus));
         
         bool goodGcError(statusMultibeam_.subarray[beamIndex].gcErrorDeg
                          <= maxPointingErrorDeg());
         
         if (!goodTrackState)
         {
            allGoodTrackState = false;
         }
         if (!goodPosition)
         {
            allGoodPosition = false;
         }
         if (!goodGcError)
         {
            allGoodGcError = false;
         }
      }

   }

   if (!tracking_ && allGoodTrackState && allGoodPosition && allGoodGcError)
   { 
      tracking_ = true;
      sseProxy_.trackingOn();
      notTrackingTargetCount_ = 0;

   }
   else if (tracking_ && !allGoodPosition)
   {
      // This is not true anymore: immediately considered off if any of the pointings are wrong
      // allow for a few fluctuations in & out of tracking before
      // reporting an error
      // JR - Dec 14, 2011 - We have determined that we can wait 10 times till we report
      // lost tracking - if an antenna is not tracking.
      if (++notTrackingTargetCount_ >= maxNotTrackingTargetCount)
      {
        tracking_ = false;
        sseProxy_.trackingOff();
        stringstream currentAzEl;
        currentAzEl << "lost target Tracking, invalid pointing. AZ "
	      << statusMultibeam_.primaryPointing[TSCOPE_BEAMXC1].azDeg << " EL " 
	      << statusMultibeam_.primaryPointing[TSCOPE_BEAMXC1].elDeg;
        reportErrorToSse(currentAzEl.str().c_str());
     }
#if 0
      // debug
      printPointingRequestsAndStatus("debug of 'lost target tracking, invalid pointing'");
#endif

   }
   else if (tracking_ && (!allGoodTrackState || !allGoodGcError))
   { 
      // allow for a few fluctuations in & out of tracking before
      // reporting an error
      if (++notTrackingTargetCount_ >= maxNotTrackingTargetCount)
      {
         tracking_ = false;  
         sseProxy_.trackingOff();

         if(!allGoodTrackState)
           reportErrorToSse("lost target tracking too many times, allGoodTrackState=false");
         if(!allGoodGcError)
           reportErrorToSse("lost target tracking too many times, allGoodGcError=false");
#if 0
         // debug
         printPointingRequestsAndStatus("debug of 'lost target tracking too many times'");
#endif

      }
   }

}

void Tscope::printPointingRequestsAndStatus(const string &header)
{
   cerr << header << endl;
   cerr << SseUtil::currentIsoDateTime() << endl;

   cerr << "tscope tune requests: " << endl;
   for(vector<TscopeTuneRequest>::iterator tuneReq = tuneRequests_.begin();
       tuneReq != tuneRequests_.end(); ++tuneReq)
   {
      const TscopeTuneRequest & request(*tuneReq);
      cerr << request;
   }

   cerr << "maxPointingErrorDeg: " << maxPointingErrorDeg() << endl;

   cerr << "beam to subarray assignments:"<< endl;
   for (vector<TscopeAssignSubarray>::iterator it = ataAssignSubarray_.begin();
        it != ataAssignSubarray_.end(); ++it)
   {
      const TscopeAssignSubarray & assignSub(*it);
      cerr << assignSub << endl;
   }

   cerr << "tscope pointing requests:" << endl;
   for(vector<TscopeSubarrayCoords>::iterator reqIt = subarrayPointingRequests_.begin();
       reqIt != subarrayPointingRequests_.end(); ++reqIt)
   {
      const TscopeSubarrayCoords & request(*reqIt);
      cerr << request << endl;
   }
   
   cerr << "tscope status:" <<endl;
   cerr << statusMultibeam_ << endl;
}

void Tscope::reportErrorToSse(const string & msg) 
{
   cerr << msg << endl;
   getSseProxy().sendErrorMsgToSse(SEVERITY_ERROR, msg);
}

void Tscope::parseArrayLine(TscopeStatusMultibeam &status, vector<string> & words)
{
/*
 Incoming line format:
  ARRAY: <HH:MM:SS UTC>

 Example:
  ARRAY: 10:21:03 UTC 
*/

   const unsigned int nExpectedWordsInLine(3);

   // indexes of data values to be extracted
   const int timeIndex(1);
   const int tzIndex(2);

   try {

      if (words.size() != nExpectedWordsInLine)
      {
	 throw SseException("incorrect number of words in line");
      }

      // Time
      // TBD validate format
      string timebuffer(words[timeIndex] + " " + words[tzIndex]);
      SseUtil::strMaxCpy(status.time, timebuffer.c_str(), 
			 MAX_TEXT_STRING);

   }
   catch (SseException & except)
   {	
      throw SseException("error parsing ARRAY status: " 
			 + string(except.descrip()));
   }

}


void Tscope::parseSubarrayLine(TscopeSubarrayStatus &status,
                               vector<string> & words)
{
/*
 Incoming line format:
-- BEAMxxx: SUBARRAY: <SLEW | TRACK | STOP | DRIVE_ERROR> WRAP <0 | 1> CAL <ON | OFF> \
--      ZFOCUS xxxx.x MHz NSCOPES xxx GCERROR xxx deg\r\n

  BEAMxxx: SUBARRAY: NANTS xx NSHAREDPOINTING xx NTRACK xx NSLEW xx \
    NSTOP xx NOFFLINE xx NERROR xx WRAP <0 | 1>  \
    ZFOCUS xxxx.x MHz GCERROR xxx deg

 Example:

--  BEAMXA1: SUBARRAY: TRACK WRAP 1 CAL OFF ZFOCUS 2205.1 MHz NSCOPES 40 \
--    GCERROR 0.00100 deg

  BEAMYD4: SUBARRAY: NANTS 42 NSHAREDPOINTING 39 NTRACK 39 NSLEW 0 NSTOP 0 \
     NOFFLINE 3 NERROR 0 WRAP 0 ZFOCUS .000000 MHz GCERROR .000000 deg

*/

   const unsigned int nExpectedWordsInLine(24);

   // indexes of data values to be extracted
   const int numAntsIndex(3);
   const int numSharedPointingIndex(5);
   const int numTrackIndex(7);
   const int numSlewIndex(9);
   const int numStopIndex(11);
   const int numOfflineIndex(13);
   const int numDriveErrorIndex(15);
   const int wrapIndex(17);
   const int zFocusIndex(19);
   const int gcErrorIndex(22);

   try {

      if (words.size() != nExpectedWordsInLine)
      {
	 throw SseException("incorrect number of words in line");
      }

      status.numTotal = SseUtil::strToInt(words[numAntsIndex]);
      status.numSharedPointing = SseUtil::strToInt(words[numSharedPointingIndex]);
      status.numTrack = SseUtil::strToInt(words[numTrackIndex]);
      status.numSlew = SseUtil::strToInt(words[numSlewIndex]);
      status.numStop = SseUtil::strToInt(words[numStopIndex]);
      status.numOffline = SseUtil::strToInt(words[numOfflineIndex]);
      status.numDriveError = SseUtil::strToInt(words[numDriveErrorIndex]);

      status.wrap = SseUtil::strToInt(words[wrapIndex]);
      status.zfocusMhz = SseUtil::strToDouble(words[zFocusIndex]);
      status.gcErrorDeg = SseUtil::strToDouble(words[gcErrorIndex]);

   }
   catch (SseException & except)
   {	
      throw SseException("error parsing SUBARRAY status: " 
			 + string(except.descrip()));
   }

}

void Tscope::parseTuningLine(TscopeTuningStatus & tuningStatus, vector<string> & words)
{
/*
  Format of incoming line:
  <tuning-name> SKYFREQ  <skyfreq> MHz

example:
  TUNINGA SKYFREQ 1420.123456 MHz 

*/
   const unsigned int nExpectedWordsInLine = 4;
   const int skyFreqValueIndex(2);

   try {

      if (words.size() != nExpectedWordsInLine)
      {
	 throw SseException("incorrect number of words in line");
      }

      tuningStatus.skyFreqMhz = 
	 SseUtil::strToDouble(words[skyFreqValueIndex]);

   } 
   catch (SseException & except)
   {
      throw SseException("error parsing tuning status: " 
			 + string(except.descrip()));
   }

}



void Tscope::parseIfChainLine(TscopeIfChainStatus & ifChainStatus, vector<string> & words)
{
/*
  Format of incoming line:
  BEAMxxx IF: <skyfreq> <attn>

example:
  BEAMXA1 IF: SKYFREQ 1420.123456 MHz ATTN 0.0 DB

*/
   const unsigned int nExpectedWordsInLine = 8;
   const int skyFreqValueIndex(3);
   const int attnValueIndex(6);

   try {

      if (words.size() != nExpectedWordsInLine)
      {
	 throw SseException("incorrect number of words in line");
      }

      ifChainStatus.skyFreqMhz = 
	 SseUtil::strToDouble(words[skyFreqValueIndex]);

      double attnDb(0);
      attnDb = SseUtil::strToDouble(words[attnValueIndex]);
     
      // TBD make use of attnDb value

   } 
   catch (SseException & except)
   {
      throw SseException("error parsing IF chain status: " 
			 + string(except.descrip()));
   }

}

void Tscope::parseBeamPointingLine(TscopePointing & pointing, vector<string> & words)
{
/* 
Format of incoming line:

  <beamname> <PRIMARY|SYNTH> <commanded-coord-sys> <az el> <ra dec> <glong glat> 

example:
   BEAMXA1: PRIMARY: J2000 AZ 120.123456 deg EL 30.123456 deg RA 20.123456 hr \
      DEC -10.234567 deg GLONG 45.123456 deg GLAT 10.123456 deg"
*/

   const unsigned int nExpectedWordsInLine = 21;  
   const int coordSysIndex(2);
   const int azIndex(4);
   const int elIndex(7);
   const int raIndex(10);
   const int decIndex(13);
   const int galLongIndex(16);
   const int galLatIndex(19);

   try {

      if (words.size() != nExpectedWordsInLine)
      {
	 throw SseException("incorrect number of words in line");
      }
      
      pointing.coordSys = SseTscopeMsg::stringToCoordSys(words[coordSysIndex]);
	    
      // az el
      pointing.azDeg = SseUtil::strToDouble(words[azIndex]);
      pointing.elDeg = SseUtil::strToDouble(words[elIndex]);
	    
      // ra dec
      pointing.raHours = SseUtil::strToDouble(words[raIndex]);
      pointing.decDeg = SseUtil::strToDouble(words[decIndex]);

      // glat glong
      pointing.galLongDeg = SseUtil::strToDouble(words[galLongIndex]);
      pointing.galLatDeg = SseUtil::strToDouble(words[galLatIndex]);

   } 
   catch (SseException & except)
   {
      throw SseException("error parsing beam pointing status: " 
			 + string(except.descrip()));
   }

}



//#define DEBUG_PARSE

#ifdef DEBUG_PARSE

// for parser debugging
static void printVector(const string & descrip, vector<string> & items)
{
   cout << descrip << ":" << endl;

   int count=0;
   for (vector<string>::iterator it=items.begin();
	it != items.end(); ++it)
   {
      cout << "(" << count << ") = {"
	   << *it << "}" << endl;
      count++;
   }
   
}
#endif


/*
  Take a line of status input, already broken into words.
  Based on the first two (key)words, dispatch the line to the appropriate
  method for further parsing.  

  Assumes status messages are of the form:

Generically:

  ARRAY: <time>
For each beam (32 total):
  BEAMxxx: SUBARRAY: <dish summary counts> <wrap> <zfocus> <gcerror>
  BEAMxxx: PRIMARY: <commanded-coord-sys> <az el> <ra dec> <glong glat>
  BEAMxxx: SYNTH: <commanded-coord-sys> <az el> <ra dec> <glong glat>
  BEAMxxx: IF: <skyfreq> 
For each tuning (4 total):
  TUNING{<tuning>} : <skyfreq>
  END

  [where BEAMxxx is BEAM{<polarity> <tuning><number>}, eg BEAMXA1]

Again, with keywords:

  ARRAY: <HH:MM:SS UTC>
For each beam (32 total):
  BEAMxxx: SUBARRAY: NANTS xx NSHAREDPOINTING xx NTRACK xx NSLEW xx \
    NSTOP xx NOFFLINE xx NERROR xx WRAP <0 | 1>  \
    ZFOCUS xxxx.x MHz GCERROR xxx deg
  BEAMxxx: PRIMARY: <AZEL | J2000 | GAL> AZ xxx deg EL xxx deg \
    RA xxx hr DEC xxx deg GLONG xxx deg GLAT xxx deg
  BEAMxxx: SYNTH: <AZEL | J2000 | GAL> AZ xxx deg EL xxx deg \
    RA xxx hr DEC xxx deg GLONG xxx deg GLAT xxx deg
  BEAMxxx: IF: SKYFREQ xxxxx.xxxxxx MHz ATTN xx dB
For each tuning (4 total):
  TUNING{A-D}: SKYFREQ xxxxx.xxxxxx MHz
  END
*/

void Tscope::dispatchStatusLineForParsing(vector<string> & words)
{
   try {

      Assert(words.size() > 0);
      if (words[0] == "END")
      {
         // End of status message was found, so report current status
         reportStatusToSse();
         return;
      }

      // All other status lines have at least 2 words
      const unsigned int minWordCount(2);
      if (words.size() < minWordCount)
      {
         throw SseException("not enough words on status line to parse");
      }
      
      // Erase trailing colon from initial keyword, so that the 
      // beam and tuning "names" match the ones that are expected internally.
      Assert(words[0].length() > 0);
      words[0].erase(words[0].length()-1);
      
      const string & keyword = words[0];
      if (keyword == "ARRAY")
      {
	 VERBOSE3(getVerboseLevel(), "parsing ARRAY line" << endl;);

	 parseArrayLine(statusMultibeam_, words);
      }
      else if (keyword.find("BEAM") != string::npos)
      {
	 VERBOSE3(getVerboseLevel(), "parsing " << keyword << " line" << endl;);

	 TscopeBeam beam = SseTscopeMsg::nameToBeam(keyword);
	 if (beam == TSCOPE_INVALID_BEAM)
	 {
	    throw SseException("invalid beam name: " + keyword);
	 }

         const string & secondaryKeyword = words[1];
         if (secondaryKeyword == "SUBARRAY:")
         {
            parseSubarrayLine(statusMultibeam_.subarray[beam], words);
         }
         else if (secondaryKeyword == "PRIMARY:")
         {
            parseBeamPointingLine(statusMultibeam_.primaryPointing[beam],
                                  words);
         }
         else if (secondaryKeyword == "SYNTH:")
         {
            parseBeamPointingLine(statusMultibeam_.synthPointing[beam],
                                  words);
         }
         else if (secondaryKeyword == "IF:")
         {
            parseIfChainLine(statusMultibeam_.ifChain[beam],
                                  words);
         }
         else 
         {
            throw SseException("invalid secondary keyword: " + secondaryKeyword);
         }

      }
      else if (keyword.find("TUNING") != string::npos)
      {
	 VERBOSE3(getVerboseLevel(), "parsing " << keyword << " line" << endl;);

	 TscopeTuning tuning = SseTscopeMsg::nameToTuning(keyword);
	 if (tuning == TSCOPE_INVALID_TUNING)
	 {
	    throw SseException("invalid tuning name: " + keyword);
	 }

	 parseTuningLine(statusMultibeam_.tuning[tuning], words);
      }
      else 
      {
	 throw SseException("unexpected keyword at beginning of line: " 
			    + keyword);
      }

   }
   catch(SseException &except)
   {
      stringstream strm;
      strm << "Error parsing tscope status: "
	   << except.descrip() << endl;

      strm << "Line: ";
      for (unsigned int i=0; i<words.size(); ++i)
      {
         strm << words[i] << " ";
      }
      strm << endl;

      reportErrorToSse(strm.str());
   }

}


/*
  Process the status message, one line at a time.
 */
void Tscope::parseStatus(const string & msg)
{
#ifdef DEBUG_PARSE
   VERBOSE3(getVerboseLevel(), "TscopeEventHandler::parse():\n" 
	    "parsing: ( " << msg << ")" << endl;);
#endif    

   // split up lines
   vector <string> lines = SseUtil::tokenize(msg, "\r\n");

#ifdef DEBUG_PARSE
   // debug
   printVector("lines", lines);
#endif
   
   for (unsigned int i = 0; i < lines.size(); i++)
   {
      vector <string> words = SseUtil::tokenize(lines[i], " \t");

      // skip empty lines
      if (words.size() == 0)
      {
	 continue;
      }

#ifdef DEBUG_PARSE
      // debug
      printVector("words", words);
#endif

      dispatchStatusLineForParsing(words);

   }

}

string Tscope::intToString(const int val)
{
   stringstream command;

   command << val;
   return command.str();
}

string Tscope::doubleToString(const double val)
{
   stringstream command;

   command.precision(6);  // N digits after the decimal
   command.setf(std::ios::fixed);  // show all decimal places up to precision

   command << val;
   return command.str();
}
  
