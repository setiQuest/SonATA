/*******************************************************************************

 File:    DxArchiverParameters.cpp
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



#include <ace/OS.h>            // to eliminate ACE errors
#include "DxArchiverParameters.h"
#include "RangeParameter.h"
#include "ChoiceParameter.h"
#include "sseDxArchiverInterface.h"
#include "Site.h"
#include "machine-dependent.h" // for float64_t et. al.
#include "DxArchiverProxy.h"
#include "DxArchiverList.h"
#include "SseArchive.h"
#include "ComponentControlImmedCmds.h"
#include "Assert.h"
#include <sstream>

extern ComponentControlImmedCmds componentControlImmedCmdsGlobal;

struct DxArchiverParametersInternal
{
   // methods:
   DxArchiverParametersInternal();  

    // default copy constructor and assignment operator are safe
   string outString_;
};



DxArchiverParametersInternal::DxArchiverParametersInternal()
{ 

}


// -- end DxArchiverParametersInternal
// -- begin DxArchiverParameters


DxArchiverParameters::DxArchiverParameters(string command) : 
   SeekerParameterGroup(command),
   internal_(new DxArchiverParametersInternal())
{
   addParameters();
   addAllImmedCmdHelp();
}

DxArchiverParameters::DxArchiverParameters(const DxArchiverParameters& rhs):
   SeekerParameterGroup(rhs.getCommand()),
   internal_(new DxArchiverParametersInternal(*rhs.internal_))
{
   setSite(rhs.getSite());
   addParameters();
}


DxArchiverParameters& DxArchiverParameters::operator=(const DxArchiverParameters& rhs)
{
   if (this == &rhs) {
      return *this;
   }
  
   setCommand(rhs.getCommand());
   eraseParamList();
   setSite(rhs.getSite());
   delete internal_;
   internal_ = new DxArchiverParametersInternal(*rhs.internal_);
   addParameters();
   return *this;
}

DxArchiverParameters::~DxArchiverParameters()
{
   delete internal_;
}

// sets all necessary parameters for class
void DxArchiverParameters::addParameters()
{

   sort();
}


// ---- DxOperation classes & methods

// A base class that runs through all the dx archivers in a list,
// executing the operation (which is deferred to the subclass).

class DxArchiverOperation
{
public:
   string runThroughDxArchiverList(const string &dxArchiverName,
				    Site *site);

   virtual ~DxArchiverOperation();
protected:
   // subclasses override this method
   virtual string callOperation(DxArchiverProxy *proxy) = 0; 

};

DxArchiverOperation::~DxArchiverOperation()
{
}


string DxArchiverOperation::runThroughDxArchiverList(
   const string &dxArchiverName, Site *site)
{
   Assert(site);
   DxArchiverList dxArchiverList;
   site->dxArchiverManager()->getProxyList(&dxArchiverList);

   string resultsString;
   bool found = false;
   for (DxArchiverList::iterator index = dxArchiverList.begin();
	index != dxArchiverList.end(); ++index)
   {
      DxArchiverProxy *dxArchiverProxy = *index;
      Assert(dxArchiverProxy);
      if ( (dxArchiverName == "all") ||
	   dxArchiverProxy->getName() == dxArchiverName)
      {
	 found = true;
	 resultsString += callOperation(dxArchiverProxy);
      }
   }

   if (found)
   {
      // if the command had no output text of its own,
      // return a generic acknowledgement.
      if (resultsString.empty())
      {
	 resultsString =  "archiver command sent.";
      }
   }
   else 
   {
      resultsString = "The requested dx archiver(s) are not connected: "
	 + dxArchiverName;
   }

   return resultsString;

}

// request status
class DxArchiverReqStat : public DxArchiverOperation
{
protected:
   virtual string callOperation(DxArchiverProxy *proxy)
   {
      proxy->requestStatusUpdate();
      return "";
   }
};

class DxArchiverResetSocket : public DxArchiverOperation
{
protected:
   virtual string callOperation(DxArchiverProxy *proxy)
   {
      proxy->resetSocket();
      return "";
   }
};


class DxArchiverShutdown : public DxArchiverOperation
{
protected:
   virtual string callOperation(DxArchiverProxy *proxy)
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

class DxArchiverRestart : public DxArchiverOperation
{
protected:
   virtual string callOperation(DxArchiverProxy *proxy)
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


class DxArchiverStatusOperation : public DxArchiverOperation
{
protected:

   virtual string callOperation(DxArchiverProxy *proxy)
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

class DxArchiverShownameOperation : public DxArchiverOperation
{
protected:

   virtual string callOperation(DxArchiverProxy *proxy)
   {
      stringstream strm;

      strm << proxy->getName() << " ";
	
      return strm.str();
   }
};


class DxArchiverIntrinsicsOperation : public DxArchiverOperation
{
protected:

   virtual string callOperation(DxArchiverProxy *proxy)
   {
      stringstream strm;

      strm << "Dx Archiver: " << proxy->getName() << endl;
      strm << proxy->getIntrinsics() << endl;
	
      return strm.str();
   }
};


// ---- immediate commands ---------

const char * DxArchiverParameters::reqstat(const char *dxArchiverName)
{
   DxArchiverReqStat reqStat;

   internal_->outString_ = reqStat.runThroughDxArchiverList(dxArchiverName, getSite());
   return internal_->outString_.c_str();
}

const char * DxArchiverParameters::shutdown(const char *dxArchiverName)
{
   SseArchive::SystemLog() << "UI request for DxArchiver shutdown: " <<
      dxArchiverName <<endl;

   DxArchiverShutdown dxArchiverShutdown;

   internal_->outString_ = 
      dxArchiverShutdown.runThroughDxArchiverList(dxArchiverName, getSite());
   return internal_->outString_.c_str();
}

const char * DxArchiverParameters::restart(const char *dxArchiverName)
{
   SseArchive::SystemLog() << "UI request for DxArchiver restart: " <<
      dxArchiverName <<endl;

   DxArchiverRestart dxArchiverRestart;

   internal_->outString_ = 
      dxArchiverRestart.runThroughDxArchiverList(dxArchiverName, getSite());
   return internal_->outString_.c_str();
}



// reset socket (disconnect proxy)
const char * DxArchiverParameters::resetsocket(const char *dxArchiverName)
{
   SseArchive::SystemLog() << "UI request for Dx socket reset: " <<
      dxArchiverName <<endl;

   DxArchiverResetSocket dxArchiverResetSocket;

   internal_->outString_ =
      dxArchiverResetSocket.runThroughDxArchiverList(dxArchiverName, getSite());
   return internal_->outString_.c_str();
}


const char *DxArchiverParameters::status(const char *dxArchiverName) const
{
   DxArchiverStatusOperation dxArchiverStatusOperation;

   internal_->outString_ = 
      dxArchiverStatusOperation.runThroughDxArchiverList(dxArchiverName, getSite());
   return internal_->outString_.c_str();

}

const char *DxArchiverParameters::names() const
{
   DxArchiverShownameOperation dxArchiverShownameOperation;

   string dxArchiverName = "all";
   internal_->outString_ = 
      dxArchiverShownameOperation.runThroughDxArchiverList(dxArchiverName, getSite());
   return internal_->outString_.c_str();

}


const char *DxArchiverParameters::intrin(const char *dxArchiverName) const
{
   DxArchiverIntrinsicsOperation dxArchiverIntrinsicsOp;

   internal_->outString_ = 
      dxArchiverIntrinsicsOp.runThroughDxArchiverList(dxArchiverName, getSite());
   return internal_->outString_.c_str();

}

void DxArchiverParameters::addAllImmedCmdHelp()
{
   addImmedCmdHelp("intrin [<archiverName>='all'] \t\tdisplay dx archiver intrinsics ");
   addImmedCmdHelp("names \t\t\t\t\tlist all connected dx archivers");
   addImmedCmdHelp("reqstat [<archiverName>='all'] \t\trequest dx archiver status update ");
   addImmedCmdHelp("resetsocket <archiverName | 'all'> \treset socket on dx archiver(s)");
   addImmedCmdHelp("restart <archiverName | 'all'> \t\trestart dx archiver(s)");
   addImmedCmdHelp("shutdown <archiverName | 'all'> \t\tshutdown dx archivers(s) ");
   addImmedCmdHelp("status [<archiverName>='all'] \t\tdisplay dx archiver status  ");
}
