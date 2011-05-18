/*******************************************************************************

 File:    ChannelizerParameters.cpp
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

/*
  ChannelizerParameters.cpp
 */

#include <ace/OS.h>            // to eliminate ACE errors
#include "ChannelizerParameters.h" 
#include "RangeParameter.h"
#include "ChoiceParameter.h"
#include "sseChannelizerInterface.h"
#include "Site.h"
#include "machine-dependent.h" // for float64_t et. al.
#include "ChannelizerProxy.h"
#include "ChannelizerList.h"
#include "ComponentControlImmedCmds.h"
#include "SseArchive.h"
#include "AtaInformation.h"
#include "Assert.h"
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

extern ComponentControlImmedCmds componentControlImmedCmdsGlobal;

struct ChannelizerParametersInternal
{
   // methods:
   ChannelizerParametersInternal();  
   RangeParameter<int32_t> delay;

    // default copy constructor and assignment operator are safe
   string outString_;
};

ChannelizerParametersInternal::ChannelizerParametersInternal():
	delay("delay","sec","Channelizer Start Delay", 3, 3, 10)
{ 

}


// -- end ChannelizerParametersInternal
// -- begin ChannelizerParameters

int32_t ChannelizerParameters::getDelay()
{
   return( internal_->delay.getCurrent() );
}

ChannelizerParameters::ChannelizerParameters(string command) : 
   SeekerParameterGroup(command),
   internal_(new ChannelizerParametersInternal())
{
   addParameters();
   addAllImmedCmdHelp();
}

ChannelizerParameters::ChannelizerParameters(const ChannelizerParameters& rhs):
   SeekerParameterGroup(rhs.getCommand()),
   internal_(new ChannelizerParametersInternal(*rhs.internal_))
{
   setSite(rhs.getSite());
   addParameters();
}


ChannelizerParameters& ChannelizerParameters::operator=(const ChannelizerParameters& rhs)
{
   if (this == &rhs) {
      return *this;
   }
  
   setCommand(rhs.getCommand());
   eraseParamList();
   setSite(rhs.getSite());
   delete internal_;
   internal_ = new ChannelizerParametersInternal(*rhs.internal_);
   addParameters();
   return *this;
}

ChannelizerParameters::~ChannelizerParameters()
{
   delete internal_;
}

// sets all necessary parameters for class
void ChannelizerParameters::addParameters()
{

   addParam(internal_->delay);
   sort();
}


// ---- ChannelizerOperation classes & methods

// A base class that runs through all the components in a list,
// executing the operation (which is deferred to the subclass).

class ChannelizerOperation
{
public:
   string runThroughChannelizerList(const string &channelizerName,
				    Site *site);

   virtual ~ChannelizerOperation();
protected:
   // subclasses override this method
   virtual string callOperation(ChannelizerProxy *proxy) = 0; 

};

ChannelizerOperation::~ChannelizerOperation()
{
}


string ChannelizerOperation::runThroughChannelizerList(
   const string &channelizerName, Site *site)
{
   Assert(site);
   ChannelizerList channelizerList;
   site->channelizerManager()->getProxyList(&channelizerList);

   string resultsString;
   bool found = false;
   for (ChannelizerList::iterator index = channelizerList.begin();
	index != channelizerList.end(); ++index)
   {
      ChannelizerProxy *channelizerProxy = *index;
      Assert(channelizerProxy);
      if ( (channelizerName == "all") || 
	   channelizerProxy->getName() == channelizerName)
      {
	 found = true;
	 resultsString += callOperation(channelizerProxy);
      }
   }

   if (found)
   {
      // if the command had no output text of its own,
      // return a generic acknowledgement.
      if (resultsString.empty())
      {
	 resultsString =  "channelizer command sent.";
      }
   }
   else 
   {
      resultsString = "The requested channelizer(s) are not connected: "
	 + channelizerName;
   }

   return resultsString;

}

// request status
class ChannelizerReqStat : public ChannelizerOperation
{
protected:
   virtual string callOperation(ChannelizerProxy *proxy)
   {
      proxy->requestStatusUpdate();
      return "";
   }
};

class ChannelizerResetSocket : public ChannelizerOperation
{
protected:
   virtual string callOperation(ChannelizerProxy *proxy)
   {
      proxy->resetSocket();
      return "";
   }
};


class ChannelizerShutdown : public ChannelizerOperation
{
protected:
   virtual string callOperation(ChannelizerProxy *proxy)
   {
      if (componentControlImmedCmdsGlobal.isComponentUnderControl(proxy->getName()))
      {
	 // Let the component controller perform a kill so that
	 // the component will not be restarted.
	 componentControlImmedCmdsGlobal.shutdown(proxy->getName());
      }
      else
      {
	 // Let the proxy handle shutdown on its own.
	 proxy->shutdown();
      }

      return "";
   }
};

class ChannelizerRestart : public ChannelizerOperation
{
protected:
   virtual string callOperation(ChannelizerProxy *proxy)
   {
      if (componentControlImmedCmdsGlobal.isComponentUnderControl(proxy->getName()))
      {
	 componentControlImmedCmdsGlobal.restart(proxy->getName());
      }
      else
      {
	 // Proxy can't do a restart
      }

      return "";
   }
};


class ChannelizerStart : public ChannelizerOperation
{
public:
   ChannelizerStart(int delaySecs, double skyFreqMhz)
      : skyFreqMhz_(skyFreqMhz)
   {
      startTime_.tv_sec = SseMsg::currentNssDate().tv_sec + delaySecs;
   }

protected:
   virtual string callOperation(ChannelizerProxy *proxy)
   {
      proxy->start(startTime_, skyFreqMhz_);
      proxy->requestStatusUpdate();

      return "";
   }

private:
   NssDate startTime_;
   double skyFreqMhz_;
};


class ChannelizerStop : public ChannelizerOperation
{
protected:
   virtual string callOperation(ChannelizerProxy *proxy)
   {
      proxy->stop();
      proxy->requestStatusUpdate();
      return "";
   }
};


class ChannelizerStatusOperation : public ChannelizerOperation
{
protected:

   virtual string callOperation(ChannelizerProxy *proxy)
   {
      stringstream strm;
      strm.precision(6);           // show N places after the decimal
      strm.setf(std::ios::fixed);  // show all decimal places up to precision

      strm << proxy->getName() << ": " << endl
	   << proxy->getCachedStatus()
	   << endl;
      return strm.str();
   }
};

class ChannelizerShownameOperation : public ChannelizerOperation
{
protected:

   virtual string callOperation(ChannelizerProxy *proxy)
   {
      stringstream strm;

      strm << proxy->getName() << " ";
	
      return strm.str();
   }
};


class ChannelizerIntrinsicsOperation : public ChannelizerOperation
{
protected:

   virtual string callOperation(ChannelizerProxy *proxy)
   {
      stringstream strm;

      strm << "Channelizer: " << proxy->getName() << endl;
      strm << proxy->getIntrinsics() << endl;
	
      return strm.str();
   }
};


// ---- immediate commands ---------

const char * ChannelizerParameters::reqstat(const char *channelizerName)
{
   ChannelizerReqStat reqStat;

   internal_->outString_ = reqStat.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();
}

const char * ChannelizerParameters::shutdown(const char *channelizerName)
{
   SseArchive::SystemLog() << "UI request for Channelizer shutdown: " <<
      channelizerName <<endl;

   ChannelizerShutdown channelizerShutdown;

   internal_->outString_ = 
      channelizerShutdown.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();
}

const char * ChannelizerParameters::restart(const char *channelizerName)
{
   SseArchive::SystemLog() << "UI request for Channelizer restart: " <<
      channelizerName <<endl;

   ChannelizerRestart channelizerRestart;

   internal_->outString_ = 
      channelizerRestart.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();
}

const char * ChannelizerParameters::start(int delaySecs, double skyFreqMhz, 
                                          const char *channelizerName)
{
   SseArchive::SystemLog() << "UI request for Channelizer start: " <<
      "Delay " << delaySecs << " Freq " << fixed << setprecision(6) <<
	 skyFreqMhz << "  " << channelizerName <<endl;

   int minDelaySecs(0);
   int maxDelaySecs(300);
   if (delaySecs < minDelaySecs || delaySecs > maxDelaySecs)
   {
      stringstream strm;
      strm << "Start delay of " << delaySecs << " secs is invalid. "
           << " Must be between " << minDelaySecs 
           << " and " << maxDelaySecs << " secs.";
      internal_->outString_ = strm.str();
   }
   else if (skyFreqMhz < AtaInformation::AtaMinSkyFreqMhz ||
            skyFreqMhz > AtaInformation::AtaMaxSkyFreqMhz)
   {
      stringstream strm;
      strm << "Start freq of " << skyFreqMhz << " MHz is invalid. "
           << " Must be between " << AtaInformation::AtaMinSkyFreqMhz
           << " and " << AtaInformation::AtaMaxSkyFreqMhz << " MHz.";
      internal_->outString_ = strm.str();
   }
   else
   {
      ChannelizerStart channelizerStart(delaySecs, skyFreqMhz);
      
      internal_->outString_ = 
         channelizerStart.runThroughChannelizerList(channelizerName, getSite());
   }

   return internal_->outString_.c_str();
}

const char * ChannelizerParameters::stop(const char *channelizerName)
{
   ChannelizerStop stop;

   internal_->outString_ = stop.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();
}


// reset socket (disconnect proxy)
const char * ChannelizerParameters::resetsocket(const char *channelizerName)
{
   SseArchive::SystemLog() << "UI request for channelizer socket reset: " <<
      channelizerName <<endl;

   ChannelizerResetSocket channelizerResetSocket;

   internal_->outString_ =
      channelizerResetSocket.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();
}


const char *ChannelizerParameters::status(const char *channelizerName) const
{
   ChannelizerStatusOperation channelizerStatusOperation;

   internal_->outString_ = 
      channelizerStatusOperation.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();

}

const char *ChannelizerParameters::names() const
{
   ChannelizerShownameOperation channelizerShownameOperation;

   string channelizerName = "all";
   internal_->outString_ = 
      channelizerShownameOperation.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();

}


const char *ChannelizerParameters::intrin(const char *channelizerName) const
{
   ChannelizerIntrinsicsOperation channelizerIntrinsicsOp;

   internal_->outString_ = 
      channelizerIntrinsicsOp.runThroughChannelizerList(channelizerName, getSite());
   return internal_->outString_.c_str();

}

void ChannelizerParameters::addAllImmedCmdHelp()
{
   addImmedCmdHelp("intrin [<chanName>='all'] - display channelizer intrinsics ");
   addImmedCmdHelp("names - list all connected channelizers");
   addImmedCmdHelp("reqstat [<chanName>='all'] - request channelizer status update ");
   addImmedCmdHelp("resetsocket <chanName | 'all'> - reset socket on channelizer(s)");
   addImmedCmdHelp("restart <chanName | 'all'> - restart channelizer(s)");
   addImmedCmdHelp("start <delay secs> <freq MHz> <chanName | 'all'> - start channelizer(s) in delay secs, tuned to freq MHz");
   addImmedCmdHelp("shutdown <chanName | 'all'> - shutdown channelizers(s) ");
   addImmedCmdHelp("status [<chanName>='all'] - display channelizer status  ");
   addImmedCmdHelp("stop <chanName>='all'> - issue stop command ");
}
