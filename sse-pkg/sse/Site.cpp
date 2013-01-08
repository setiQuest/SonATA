/*******************************************************************************

 File:    Site.cpp
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


// The Site monitors & provides access to the components
// for the system.

#include "ace/Reactor.h"
#include "Site.h"
#include "DxProxy.h"
#include "DxArchiverProxy.h"
#include "ChannelizerProxy.h"
#include "IfcProxy.h"
#include "TscopeProxy.h"
#include "TestSigProxy.h"
#include "ComponentControlProxy.h"
#include "NssAcceptHandler.h"
#include "DxList.h"
#include "DxArchiverList.h"
#include "ChannelizerList.h"
#include "IfcList.h"
#include "TscopeList.h"
#include "TestSigList.h"
#include "ComponentControlList.h"
#include "Subscriber.h"
#include "SseArchive.h"
#include "SseSystem.h"
#include "SseMessage.h"
#include "ExpectedNssComponentsTree.h"
#include "DxComponentManager.h"
#include "SseComponentManager.h"
#include "SiteSubscriber.h"
#include "SseAstro.h"
#include "PermRfiMaskFilename.h"
#include "MsgSender.h"
#include "SseTscopeMsg.h"
#include "SseChanMsg.h"
#include "Timeout.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

// various configuration files.  
// TBD these should probably be adjustable via the UI
static const string SystemStatusFilename("sse-system-status.txt");

using namespace std;

// --- declarations for private static routines -----

struct SiteInternal
{
   SiteInternal(Site *site,
		const string &dxArchiver1Hostname,
		const string &dxToDxArchiver1Port,
		const string &dxArchiver2Hostname,
		const string &dxToDxArchiver2Port,
		const string &dxArchiver3Hostname,
		const string &dxToDxArchiver3Port,
		bool noUi
      );

   ~SiteInternal();


   void loadExpectedNssComponentsTree(const string &configFilename);
   void loadExpectedComponentsIntoComponentStatusMap();
   void loadAtaBeamStatusIndices();

   void eraseComponentStatusMap();
   void resetComponentStatusMap();
   void storeComponentStatus(const string &componentName,
			     const string &status);
   void printSystemStatus(ostream &strm);
   void updateDxOnelineStatus(const string &prefix,
			       DxList *dxList);
   void updateDxArchiverOnelineStatus(const string &prefix,
				       DxArchiverList *dxArchiverList);
   void updateChannelizerOnelineStatus(const string &prefix,
				       ChannelizerList *channelizerList);
   void updateIfcOnelineStatus(const string &prefix,
			       IfcList *ifcList);
   void updateTscopeStatus(const string &prefix,
				  TscopeList *tscopeList);
   void updateTestSigOnelineStatus(const string &prefix,
				   TestSigList *testSigList);
   void updateComponentControlOnelineStatus(const string &prefix,
				   ComponentControlList *componentControlListx);
   void printComponentStatusMap(ostream &strm);

   string getIfcNameForDx(const string &dxName);
   string getBeamNameForDx(const string &dxName);

   // --- data ---

   NssComponentManager<DxProxy> *dxManager_;
   NssComponentManager<DxProxy> *zxManager_;
   NssComponentManager<DxArchiverProxy> *dxArchiverManager_;
   NssComponentManager<ChannelizerProxy> *channelizerManager_;    
   NssComponentManager<IfcProxy> *ifcManager_;    
   NssComponentManager<TscopeProxy> *tscopeManager_;    
   NssComponentManager<TestSigProxy> *testSigManager_;    
   NssComponentManager<ComponentControlProxy> *componentControlManager_;    

   NssAcceptHandler<DxProxy> *dxAcceptHandler_;
   NssAcceptHandler<DxProxy> *zxAcceptHandler_;
   NssAcceptHandler<DxArchiverProxy> *dxArchiverAcceptHandler_;
   NssAcceptHandler<ChannelizerProxy> *channelizerAcceptHandler_;
   NssAcceptHandler<IfcProxy> *ifcAcceptHandler_;
   NssAcceptHandler<TscopeProxy> *tscopeAcceptHandler_;
   NssAcceptHandler<TestSigProxy> *testSigAcceptHandler_;
   NssAcceptHandler<ComponentControlProxy> *componentControlAcceptHandler_;


   Site *site_;
   int verboseLevel_;
   ofstream systemStatusStrm_;
   ACE_Recursive_Thread_Mutex objectMutex_; 
   ACE_Recursive_Thread_Mutex compStatusMapMutex_; 
   ACE_Recursive_Thread_Mutex configMutex_; 
   SiteSubscriber subscriber_;
   ExpectedNssComponentsTree *expectedNssComponentsTree_;
   string expectedNssComponentsConfigFilename_;

   typedef map<string, string> ComponentStatusMap;
   ComponentStatusMap compStatusMap_;
   bool noUi_;
   vector<TscopeBeam> ataBeamStatusIndices_;
   NssDate lastStatusTime_;
   int numberOfStatusChanges_;
};


SiteInternal::SiteInternal(Site *site,
			   const string &dxArchiver1Hostname,
			   const string &dxToDxArchiver1Port,
			   const string &dxArchiver2Hostname,
			   const string &dxToDxArchiver2Port,
			   const string &dxArchiver3Hostname,
			   const string &dxToDxArchiver3Port,
			   bool noUi
   )
   :
   dxManager_(0),
   zxManager_(0),
   dxArchiverManager_(0),
   channelizerManager_(0),
   ifcManager_(0),
   tscopeManager_(0),
   testSigManager_(0),
   dxAcceptHandler_(0),
   zxAcceptHandler_(0),
   dxArchiverAcceptHandler_(0),
   channelizerAcceptHandler_(0),
   ifcAcceptHandler_(0),
   tscopeAcceptHandler_(0),
   testSigAcceptHandler_(0),
   componentControlAcceptHandler_(0),
   site_(site),
   verboseLevel_(0),
   subscriber_(site),
   expectedNssComponentsTree_(0),
   noUi_(noUi),
   numberOfStatusChanges_(0)
{

   lastStatusTime_.tv_sec = SseMsg::currentNssDate().tv_sec;

   // create all the component managers
   dxManager_ = new DxComponentManager(
      &subscriber_, 
      dxArchiver1Hostname, dxToDxArchiver1Port,
      dxArchiver2Hostname, dxToDxArchiver2Port,
      dxArchiver3Hostname, dxToDxArchiver3Port,
      DxPermRfiMaskFilename);    
   dxManager_->setManagerName("seekerDxManager");

   zxManager_ = new DxComponentManager(
      &subscriber_, 
      dxArchiver1Hostname, dxToDxArchiver1Port,
      dxArchiver2Hostname, dxToDxArchiver2Port,
      dxArchiver3Hostname, dxToDxArchiver3Port,
      DxPermRfiMaskFilename);    
   zxManager_->setManagerName("seekerZxManager");

   dxArchiverManager_ = new SseComponentManager<DxArchiverProxy>(&subscriber_);
   channelizerManager_ = new SseComponentManager<ChannelizerProxy>(&subscriber_);    
   ifcManager_ = new SseComponentManager<IfcProxy>(&subscriber_);    
   tscopeManager_ = new SseComponentManager<TscopeProxy>(&subscriber_);    
   testSigManager_ = new SseComponentManager<TestSigProxy>(&subscriber_);    
   componentControlManager_ = new SseComponentManager<ComponentControlProxy>(&subscriber_);    
   
// if (!noUi_)
   {
      try {
	 SseUtil::openOutputFileStream(
	    systemStatusStrm_, SseArchive::getArchiveTemplogsDir() +
	    "/" + SystemStatusFilename);
      }
      catch (SseException &except)
      {
	 cerr << "SiteInternal: warning " << except << endl;
      }
   }
}

SiteInternal::~SiteInternal()
{
   //if (!noUi_)
   {
      systemStatusStrm_.close();
   }

   // delete accept handlers

   delete dxAcceptHandler_;
   delete zxAcceptHandler_;
   delete dxArchiverAcceptHandler_;
   delete channelizerAcceptHandler_;
   delete ifcAcceptHandler_;
   delete tscopeAcceptHandler_;
   delete testSigAcceptHandler_;
   delete componentControlAcceptHandler_;

   // delete component managers
    
   delete zxManager_;
   delete dxManager_;
   delete dxArchiverManager_;
   delete channelizerManager_;
   delete ifcManager_; 
   delete tscopeManager_; 
   delete testSigManager_; 
   delete componentControlManager_;

   // misc
   delete expectedNssComponentsTree_;

}

// -------------- Site methods  -----------------------------------

Site::Site(const string &dxPort,
           const string &zxPort,
	   const string &dxArchiverPort,
	   const string &channelizerPort, 
	   const string &dxArchiver1Hostname,
	   const string &dxToDxArchiver1Port,
	   const string &dxArchiver2Hostname,
	   const string &dxToDxArchiver2Port,
	   const string &dxArchiver3Hostname,
	   const string &dxToDxArchiver3Port,
	   const string &ifcPort,
	   const string &tscopePort,
	   const string &testSigPort,
	   const string &componentControlPort,
	   const string &expectedNssComponentConfigFilename,
	   bool noUi)
   :internal_(new SiteInternal(this,
			       dxArchiver1Hostname,
			       dxToDxArchiver1Port,
			       dxArchiver2Hostname,
			       dxToDxArchiver2Port,
			       dxArchiver3Hostname,
			       dxToDxArchiver3Port,
			       noUi)),
   systemStatusRepeatTimer_("status repeat timer")
{
   int initialWaitTimeSecs(10);
   int verboseLevel(0);
   int repeatIntervalSecs(2);
   systemStatusRepeatTimer_.setRepeatIntervalSecs(repeatIntervalSecs);
   systemStatusRepeatTimer_.startTimer( initialWaitTimeSecs, this,
	&Site::repeatSystemStatus, verboseLevel);


   // create all the component accept handlers
   // these will throw an exception on failure.

   internal_->dxAcceptHandler_ =
      new NssAcceptHandler<DxProxy>(dxPort, dxManager());
   internal_->zxAcceptHandler_ =
      new NssAcceptHandler<DxProxy>(zxPort, zxManager());
   internal_->dxArchiverAcceptHandler_ =
      new NssAcceptHandler<DxArchiverProxy>(dxArchiverPort, dxArchiverManager());
   internal_->channelizerAcceptHandler_ = 
      new NssAcceptHandler<ChannelizerProxy>(channelizerPort, channelizerManager());

   internal_->ifcAcceptHandler_ = 
      new NssAcceptHandler<IfcProxy>(ifcPort, ifcManager());

   internal_->tscopeAcceptHandler_ = 
      new NssAcceptHandler<TscopeProxy>(tscopePort, tscopeManager());

   internal_->testSigAcceptHandler_ = 
      new NssAcceptHandler<TestSigProxy>(testSigPort, testSigManager());

   internal_->componentControlAcceptHandler_ = 
      new NssAcceptHandler<ComponentControlProxy>(componentControlPort, componentControlManager());

   internal_->expectedNssComponentsConfigFilename_ = expectedNssComponentConfigFilename;

   /*
     Make an initial empty component tree for convenience, just so one exists.
     This is a fallback in case the file load fails.
   */
   internal_->expectedNssComponentsTree_ = new ExpectedNssComponentsTree();
   // now try to load from file
   loadExpectedComponentsConfig(internal_->expectedNssComponentsConfigFilename_);
}


// Default constructor is just used for limited testing,
// so give the SiteInternal constructor some dummy archiver ports
static const char *dummyDxArchiver1Hostname = "localhost";
static const char *dummyDxToDxArchiver1Port = "9998";
static const char *dummyDxArchiver2Hostname = "localhost";
static const char *dummyDxToDxArchiver2Port = "9998";
static const char *dummyDxArchiver3Hostname = "localhost";
static const char *dummyDxToDxArchiver3Port = "9998";
static const bool dummyNoUi = true;

Site::Site(vector<string> &expectedNssComponentsConfigVector)
   :internal_(new SiteInternal(this,
			       dummyDxArchiver1Hostname,
			       dummyDxToDxArchiver1Port,
			       dummyDxArchiver2Hostname,
			       dummyDxToDxArchiver2Port,
			       dummyDxArchiver3Hostname,
			       dummyDxToDxArchiver3Port,
			       dummyNoUi
      )),
   systemStatusRepeatTimer_("status repeat timer")
{

   int initialWaitTimeSecs(10);
   int verboseLevel(0);
   int repeatIntervalSecs(7);
   systemStatusRepeatTimer_.setRepeatIntervalSecs(repeatIntervalSecs);
   systemStatusRepeatTimer_.startTimer( initialWaitTimeSecs, this,
	&Site::repeatSystemStatus, verboseLevel);

   internal_->expectedNssComponentsTree_ =
      new ExpectedNssComponentsTree(expectedNssComponentsConfigVector);

   internal_->loadExpectedComponentsIntoComponentStatusMap();

   internal_->loadAtaBeamStatusIndices();
}

Site::~Site()
{
   VERBOSE2(internal_->verboseLevel_, "Site destructor" << endl;);

   delete internal_;

}

/*
  Configure the expected components.  If arg is "" then use default filename.
 */
void Site::loadExpectedComponentsConfig(const string &expectedComponentsFilename)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->configMutex_);

   string newConfigFile = internal_->expectedNssComponentsConfigFilename_;
   if (expectedComponentsFilename != "")
   {
      newConfigFile = expectedComponentsFilename;
   }

   internal_->loadExpectedNssComponentsTree(newConfigFile);
   internal_->loadExpectedComponentsIntoComponentStatusMap();
   internal_->loadAtaBeamStatusIndices();
   
   /*
     Connect the expectedNssComponentsTree_ to the dxManager_.
     Need to downcast since this method is not available in the
     base class.
   */   
   DxComponentManager *pcm = dynamic_cast<DxComponentManager*>(
      internal_->dxManager_);
   Assert(pcm);
   Assert(internal_->expectedNssComponentsTree_);
   pcm->setExpectedNssComponentsTree(internal_->expectedNssComponentsTree_);
// Do the same for the Zx
   DxComponentManager *zcm = dynamic_cast<DxComponentManager*>(
      internal_->zxManager_);
   cerr << "Assert(zcm)" << endl;
   Assert(zcm);
   cerr << "Assert(internal->expectedNssComponensTree_)" << endl;
   Assert(internal_->expectedNssComponentsTree_);
   cerr << "setExpectedNssComponentsTree" << endl;
   zcm->setExpectedNssComponentsTree(internal_->expectedNssComponentsTree_);
   cerr << "finished set" << endl;
}

ExpectedNssComponentsTree * Site::getExpectedNssComponentsTree()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->configMutex_);

   return internal_->expectedNssComponentsTree_;
}

// Return an NssComponentTree, allocated on the heap,
// containing proxies for all the stored components. 
// The caller is responsible for freeing the tree.
NssComponentTree * Site::getAllComponents() const
{
   // Don't use a mutex here -- the component managers protect themselves
   DxList dxList;
   internal_->dxManager_->getProxyList(&dxList);

   DxList zxList;
   internal_->zxManager_->getProxyList(&zxList);
   ChannelizerList chanList;
   internal_->channelizerManager_->getProxyList(&chanList);

   IfcList ifcList;
   internal_->ifcManager_->getProxyList(&ifcList);

   TscopeList tscopeList;
   internal_->tscopeManager_->getProxyList(&tscopeList);

   TestSigList testSigList;
   internal_->testSigManager_->getProxyList(&testSigList);
   NssComponentTree *tree = new NssComponentTree(
      dxList,
      zxList,
      chanList,
      ifcList, 
      tscopeList,	
      testSigList, 
      internal_->expectedNssComponentsTree_);

   return tree;
}



NssComponentManager<DxProxy> *Site::dxManager()
{
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->dxManager_;
}

NssComponentManager<DxProxy> *Site::zxManager()
{
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->zxManager_;
}

NssComponentManager<DxArchiverProxy> *Site::dxArchiverManager()
{
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->dxArchiverManager_;
}

NssComponentManager<ChannelizerProxy> *Site::channelizerManager()
{
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->channelizerManager_;
}


NssComponentManager<IfcProxy> *Site::ifcManager()
{
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->ifcManager_;
}

NssComponentManager<TscopeProxy> *Site::tscopeManager()
{ 
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->tscopeManager_;
}

NssComponentManager<TestSigProxy> *Site::testSigManager()
{ 
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->testSigManager_;
}

NssComponentManager<ComponentControlProxy> *Site::componentControlManager()
{ 
   // Don't use a mutex here -- the component managers protect themselves

   return internal_->componentControlManager_;
}


void Site::setVerbose(int verboseLevel)
{
   {
      ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

      internal_->verboseLevel_ = verboseLevel;
   }

   // Don't use a mutex here -- the component managers protect themselves

   dxManager()->setVerbose(verboseLevel);
   zxManager()->setVerbose(verboseLevel);
   dxArchiverManager()->setVerbose(verboseLevel);
   channelizerManager()->setVerbose(verboseLevel);
   ifcManager()->setVerbose(verboseLevel);
   tscopeManager()->setVerbose(verboseLevel);
   testSigManager()->setVerbose(verboseLevel);
   componentControlManager()->setVerbose(verboseLevel);

}

int Site::getVerbose() const
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->objectMutex_);

   return internal_->verboseLevel_;
}

void Site::printSystemStatus(ostream &strm)
{
   internal_->printSystemStatus(strm);
}

void Site::updateSystemStatus()
{
//   if (! internal_->noUi_)
   {
      internal_->numberOfStatusChanges_++;
      internal_->printSystemStatus(internal_->systemStatusStrm_);
   }
}

void Site::repeatSystemStatus()
{
//   if (! internal_->noUi_)
   {
      internal_->printSystemStatus(internal_->systemStatusStrm_);
   }
}


// ---- private static routines ----------------
// ---------------------------------------------

// print the name & status for all components
void SiteInternal::printComponentStatusMap(ostream &strm)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(compStatusMapMutex_);

   // save old format flags
   ios::fmtflags oldFlags = strm.flags();

   // left adjust the field.
   // this stays in effect until altered.
   strm << resetiosflags(ios::adjustfield);  // clear flags
   strm << setiosflags(ios::left);  // set justification

   const int nameWidth = 14;
   const int statusWidth = 60;

   // print the header
   strm << "==================================================" << endl;
   strm << "NSS System Status: " << SseUtil::currentIsoDateTime() << endl;
   strm << endl;
   strm << setw(nameWidth) << "Component" 
	<< setw(statusWidth) << "Status" << endl;
   strm << setw(nameWidth) << "---------" 
	<< setw(statusWidth) << "------" << endl;

   // print each component name & its status
   ComponentStatusMap::iterator it;
   for (it = compStatusMap_.begin(); it != compStatusMap_.end(); ++it)
   {

      // for g++, setw doesn't seem to work with strings, but
      // does work with c_str()
      if ( strncmp( it->first.c_str(), "ifc1",3) != 0 )
      {
      strm << setw(nameWidth) << it->first.c_str()  // print component name
	   << setw(statusWidth) << it->second.c_str() // print status
	   << endl;
       }
   }    

   // restore saved format flags
   strm.flags(oldFlags);

}

void SiteInternal::eraseComponentStatusMap()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(compStatusMapMutex_);

   compStatusMap_.clear();
}

void SiteInternal::resetComponentStatusMap()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(compStatusMapMutex_);

   ComponentStatusMap::iterator it;
   for (it = compStatusMap_.begin(); it != compStatusMap_.end(); ++it)
   {
      it->second = "Offline";
   }    

}

void SiteInternal::storeComponentStatus(const string &componentName,
					const string &status)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(compStatusMapMutex_);

   compStatusMap_[componentName] = status;
}


// print a status summary of the components of the system
void SiteInternal::printSystemStatus(ostream &strm)
{
   NssDate currentTime;
   currentTime.tv_sec = SseMsg::currentNssDate().tv_sec;
   if ( (currentTime.tv_sec - lastStatusTime_.tv_sec) < 1) return;
   if ( numberOfStatusChanges_ == 0 ) return;
   numberOfStatusChanges_ = 0;
   lastStatusTime_.tv_sec = currentTime.tv_sec;
   // Clean out all the old status values.
   // This takes care of the case where a component disconnects.
   resetComponentStatusMap();

   const string & prefix("");

   DxList dxList;
   dxManager_->getProxyList(&dxList);
   updateDxOnelineStatus(prefix, &dxList);
   DxList zxList;
   zxManager_->getProxyList(&zxList);
   updateDxOnelineStatus(prefix, &zxList);

   DxArchiverList dxArchiverList;
   dxArchiverManager_->getProxyList(&dxArchiverList);
   updateDxArchiverOnelineStatus(prefix, &dxArchiverList);

   ChannelizerList channelizerList;
   channelizerManager_->getProxyList(&channelizerList);
   updateChannelizerOnelineStatus(prefix, &channelizerList);

   IfcList ifcList;
   ifcManager_->getProxyList(&ifcList);
   //updateIfcOnelineStatus(prefix, &ifcList);

   TscopeList tscopeList;
   tscopeManager_->getProxyList(&tscopeList);
   updateTscopeStatus(prefix, &tscopeList);

   TestSigList testSigList;
   testSigManager_->getProxyList(&testSigList);
   updateTestSigOnelineStatus(prefix, &testSigList);

   ComponentControlList componentControlList;
   componentControlManager_->getProxyList(&componentControlList);
   updateComponentControlOnelineStatus(prefix, &componentControlList);

   printComponentStatusMap(strm);
}

string SiteInternal::getIfcNameForDx(const string &dxName)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(configMutex_);

   const string defaultIfcName("ifc??");

   Assert(expectedNssComponentsTree_);
   string ifcName(expectedNssComponentsTree_->getIfcForDx(dxName));
   if (ifcName == "")
   {
      ifcName = defaultIfcName;
   }

   return ifcName;
}

string SiteInternal::getBeamNameForDx(const string &dxName)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(configMutex_);

   const string defaultBeamName("beam??");

   Assert(expectedNssComponentsTree_);
   string beamName(expectedNssComponentsTree_->getBeamForDx(dxName));
   if (beamName == "")
   {
      beamName = defaultBeamName;
   }

   return beamName;
}

static bool nameUnknown(const string &name)
{
   return (name.find("unknown") != string::npos);
}


void SiteInternal::updateDxOnelineStatus(const string &prefix,
					  DxList *dxList)
{
   // no mutex needed

   // update one line of status for each proxy in the list

   for (DxList::iterator index = dxList->begin();
	index != dxList->end();
	++index)
   {
      DxProxy *proxy = *index;

      const string dxName(proxy->getName());

      // skip messages from unknown dxs.  This should
      // only happen when they first connect, but have 
      // not yet identified themselves.
      if (! nameUnknown(dxName))
      {
	 stringstream strm;
	 strm.precision(6);           // show N places after the decimal
	 strm.setf(std::ios::fixed);  // show all decimal places up to precision

	 strm << prefix
	      << "("  << getBeamNameForDx(dxName) << ") "
	      << proxy->getCachedDxStatus()
	      << "Sky: " << proxy->getDxSkyFreq() << " MHz"
	      << " Chan: " << proxy->getChannelNumber();

	 storeComponentStatus(dxName, strm.str());
      }
   }

}


void SiteInternal::updateDxArchiverOnelineStatus(const string &prefix,
						  DxArchiverList *dxArchiverList)
{
   // no mutex needed

   // update one line of status for each proxy in the list

   for (DxArchiverList::iterator index = dxArchiverList->begin();
	index != dxArchiverList->end();
	++index)
   {
      DxArchiverProxy *proxy = *index;

      const string componentName(proxy->getName());

      // skip messages from unknown DxArchivers.  This should
      // only happen when they first connect, but have 
      // not yet identified themselves.
      if (! nameUnknown(componentName))
      {
	 stringstream strm;
	 const DxArchiverStatus & status = proxy->getCachedStatus();

	 strm << prefix
	      << status.timestamp
	      << "  " << "# Connected Dxs: " 
	      << status.numberOfConnectedDxs;

	 // store the status 
	 storeComponentStatus(componentName, strm.str());
      }
   }

}

void SiteInternal::updateChannelizerOnelineStatus(const string &prefix,
						  ChannelizerList *channelizerList)
{
   // no mutex needed

   // update one line of status for each proxy in the list

   for (ChannelizerList::iterator index = channelizerList->begin();
	index != channelizerList->end();
	++index)
   {
      ChannelizerProxy *proxy = *index;

      const string componentName(proxy->getName());

      // skip messages from unknown components.  This should
      // only happen when they first connect, but have 
      // not yet identified themselves.
      if (! nameUnknown(componentName))
      {
	 stringstream strm;
	 const ssechan::Status & status = proxy->getCachedStatus();

	 strm << prefix
	      << status.timestamp << "  " 
              << "State: " << SseChanMsg::stateToString(status.state) << "  "
              << "Start: " << status.startTime << "  " << 
               fixed << setprecision(6) 
              << "Sky: " << status.centerSkyFreqMhz << " MHz ";

	 storeComponentStatus(componentName, strm.str());
      }
   }
}

static void printStxStatus(ostream &strm, IfcProxy *ifcProxy)
{
   strm << "STX: ";
   if (ifcProxy->goodStxStatus())
   {
      strm << "OK";
   }
   else
   {
      strm << "ERROR";
   }

   strm << " (Stat: ";
   strm << ifcProxy->getStxStatusString();
   strm << ") ";	  

}

void SiteInternal::updateIfcOnelineStatus(const string &prefix,
					  IfcList *ifcList)
{
   // no mutex needed

   // update one line of status for each proxy in the list

   for (IfcList::iterator index = ifcList->begin();
	index != ifcList->end();
	++index)
   {
      IfcProxy *ifcProxy = *index;
      const string name(ifcProxy->getName());

      // skip messages from unknown ifcs.  This should
      // only happen when they first connect, but have 
      // not yet identified themselves.
      if (! nameUnknown(name))
      {
	 stringstream strm;

	 const IfcStatus status = ifcProxy->getStatus();

	 strm.precision(1);  // show N places after the decimal
	 strm.setf(std::ios::fixed);  // show all decimal places up to precision

	 strm << prefix
	      << status.timeStamp
	      << "  ";

	 printStxStatus(strm, ifcProxy);

	 strm << "Src: " << status.ifSource << "  ";

	 // STX variance
	 strm << "LVar: " << status.stxVarLeft
	      << " RVar: " << status.stxVarRight;

	 // TBD status info
	 storeComponentStatus(name, strm.str());
      }
   }

}

static string prepareSubarrayStatus(
   const string &name, 
   const string &indent,
   const TscopeSubarrayStatus &subarrayStatus)
{
   stringstream strm;
   
   strm.precision(3);           // show N places after the decimal
   strm.setf(std::ios::fixed);  // show all decimal places up to precision
   
   strm << name << indent

        << "GCErr:" << subarrayStatus.gcErrorDeg << "d"
        << " TotalAnts:" << subarrayStatus.numTotal 
        << " InPrimary:" << subarrayStatus.numSharedPointing
        << " Track:" << subarrayStatus.numTrack
        << " Slew:" << subarrayStatus.numSlew
        << " Stop:" << subarrayStatus.numStop
        << " Off:" << subarrayStatus.numOffline
        << " DriveErr:" << subarrayStatus.numDriveError
        << "";
#if 0
// TBD - update subarray status

	 strm << "Zfocus: " << status.zfocusMhz << " MHz  ";

	 strm << "Wrap:" << status.wrap << "  ";
#endif

   return strm.str();
}
static string prepareBeamStatus(
   const string &beamName, 
   const string &indent,
   const TscopePointing & pointing)
{
   stringstream strm;
   
   strm.precision(3);           // show N places after the decimal
   strm.setf(std::ios::fixed);  // show all decimal places up to precision
   
   strm << beamName << indent

	<< "Cmd:" << SseTscopeMsg::coordSysToString(pointing.coordSys) << " "

	<< "RA:" << pointing.raHours << "h " 
	<< "Dec:" << pointing.decDeg << "d "
      
	<< "GLon:" << pointing.galLongDeg << "d "
	<< "GLat:" << pointing.galLatDeg << "d "
   
	<< "Az:" << pointing.azDeg << "d " 
	<< "El:" << pointing.elDeg << "d ";

   return strm.str();
}

void SiteInternal::updateTscopeStatus(const string &prefix,
				      TscopeList *tscopeList)
{
   // no mutex needed

   // print status for beams & tunings for each proxy in the list

   for (TscopeList::iterator index = tscopeList->begin();
	index != tscopeList->end(); ++index)
   {
      TscopeProxy *proxy = *index;
      const string name(proxy->getName());

      // skip messages from unknown tscopes.  This should
      // only happen when they first connect, but have 
      // not yet identified themselves.

      if (! nameUnknown(name))
      {
	 stringstream strm;

	 strm.precision(1);           // show N places after the decimal
	 strm.setf(std::ios::fixed);  // show all decimal places up to precision
	 strm << prefix;

	 // pull out the status info
	 const TscopeStatusMultibeam status = proxy->getStatus();
	 strm << status.time << "  ";

	 string indent("\t\t");
	 strm << "Tunings:  ";

	 // RF tunings
	 for (int tuningIndex=0; tuningIndex < TSCOPE_N_TUNINGS;
	      ++tuningIndex)
	 {
	    // use last letter of tuning name as abbreviation
	    // eg, TUNINGA -> A

	    string tuningName(SseTscopeMsg::tuningToName(
	       static_cast<TscopeTuning>(tuningIndex)));

	    int lastCharIndex(tuningName.length()-1);
	    Assert(lastCharIndex >= 0);

	    strm << tuningName.at(lastCharIndex)
		 << ":"
		 << status.tuning[tuningIndex].skyFreqMhz 
		 << " MHz  ";
	 }

         /*
           Add status for every synthesized ata beam in the config file.
	   Also show primary beam status, based on the ATA array information
           associated with the first synth beam.
           This assumes that all synth beams share the same primary beam.
         */
         bool firstBeam=true;
	 for (vector<TscopeBeam>::iterator it = ataBeamStatusIndices_.begin();
	      it != ataBeamStatusIndices_.end(); ++it)
	 {
	    TscopeBeam beamIndex = *it;
	    Assert(beamIndex != TSCOPE_INVALID_BEAM);
	    const string ataBeamName(
	       SseUtil::strToLower(SseTscopeMsg::beamToName(beamIndex)));

            if (firstBeam)
            {
               strm << endl
                    << prepareSubarrayStatus("array", indent, 
                                             status.subarray[beamIndex]);

               strm << endl
                    << prepareBeamStatus("primary", indent, 
                                         status.primaryPointing[beamIndex]);

               firstBeam = false;
            }
	    
	    strm << endl
		 << prepareBeamStatus(ataBeamName, indent, 
                                      status.synthPointing[beamIndex]);

	 }

	 storeComponentStatus(name, strm.str());
      }
   }

}

void SiteInternal::updateTestSigOnelineStatus(const string &prefix,
					      TestSigList *testSigList)
{

//    cout << "site::updateTestSigOnelineStatus" << endl;

   // no mutex needed

   // print one line of status for each proxy in the list

   for (TestSigList::iterator index = testSigList->begin();
	index != testSigList->end();
	++index)
   {
      TestSigProxy *proxy = *index;
      const string name(proxy->getName());

      // skip messages from unknown testSigs.  This should
      // only happen when they first connect, but have 
      // not yet identified themselves.
      if (! nameUnknown(name))
      {
	 stringstream strm;
	 
	 const int HzPrecision(6);
	 strm.precision(HzPrecision);  // show N places after the decimal
	 strm.setf(std::ios::fixed);  // show all decimal places up to precision
	 strm << prefix;
	 strm << proxy->getStatus().timeStamp;
	 strm << "  ";

	 strm << "Freq: " 
	      << proxy->getStatus().testSignal.driftTone.start.frequency
	      << " MHz  ";

	 strm.precision(2);
	 strm << "Drift: " 
	      << proxy->getStatus().testSignal.driftTone.driftRate
	      << " Hz/s  ";
	  
	 strm << "SigType: "
	      << proxy->getTestSignalType()  // cw or pulse
	      << " ";

	 storeComponentStatus(name, strm.str());
      }
   }

}


void SiteInternal::updateComponentControlOnelineStatus(
   const string &prefix,
   ComponentControlList *componentControlList)
{

//    cout << "site::updateComponentControlOnelineStatus" << endl;

   // no mutex needed

   // print one line of status for each proxy in the list

   for (ComponentControlList::iterator index = componentControlList->begin();
	index != componentControlList->end();
	++index)
   {
      ComponentControlProxy *proxy = *index;
      const string name(proxy->getName());

      // skip messages from unknown componentControls.  This should
      // only happen when they first connect, but have 
      // not yet identified themselves.
      if (! nameUnknown(name))
      {
	 stringstream strm;

	 strm.precision(1);           // show N places after the decimal
	 strm.setf(std::ios::fixed);  // show all decimal places up to precision

	 strm << proxy->getStatus().timeStamp;

	 storeComponentStatus(name, strm.str());
      }
   }

}

// Load the expected NSS components from a config file.
// configFile name is expected to exist in the SSE "setup" directory.

void SiteInternal::loadExpectedNssComponentsTree(const string &configFilename)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(configMutex_);

   string fullPathConfigFilename = SseSystem::getSetupDir() + "/" +
      configFilename;

   // make sure the file is there
   if (! SseUtil::fileIsReadable(fullPathConfigFilename))
   {
      stringstream strm;
      strm << "Can't read 'expected components' config file "
	   << fullPathConfigFilename << endl;
      SseMessage::log(MsgSender,
                      NSS_NO_ACTIVITY_ID, SSE_MSG_FILE_ERROR,
                      SEVERITY_WARNING, strm.str().c_str(),
                      __FILE__, __LINE__);

      cerr << strm.str();
   } 
   else
   {
      stringstream infoStrm;
      infoStrm << "Loading 'expected components' config file: " 
               << fullPathConfigFilename << endl;

      SseArchive::SystemLog() << infoStrm.str();
      cerr << infoStrm.str();

      // load from the file
      delete expectedNssComponentsTree_;

      stringstream errorStrm;
      expectedNssComponentsTree_ =
	 new ExpectedNssComponentsTree(fullPathConfigFilename, errorStrm);

      if (errorStrm.str() != "")
      {
	 // log may not be visible in UI when this is printed, so
	 cerr << errorStrm.str() << endl;

	 SseMessage::log(MsgSender,
                         NSS_NO_ACTIVITY_ID, SSE_MSG_FILE_ERROR,
                         SEVERITY_WARNING, errorStrm.str(),
                         __FILE__, __LINE__);
      }

   }

   // Debug
   //cout << *expectedNssComponentsTree_;
}


// Preload selected "expected" components into the component
// status map.
void SiteInternal::loadExpectedComponentsIntoComponentStatusMap()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(configMutex_);

   vector<string> componentNames;

   Assert(expectedNssComponentsTree_);
   vector<string> ifcNames(expectedNssComponentsTree_->getIfcs());
   componentNames.insert(componentNames.end(), ifcNames.begin(),
			 ifcNames.end());

   vector<string> dxNames(expectedNssComponentsTree_->getDxs());
   componentNames.insert(componentNames.end(), dxNames.begin(),
			 dxNames.end());

   eraseComponentStatusMap();

   for (size_t i=0; i<componentNames.size(); ++i)
   {
      const string initialStatus("Offline");
      storeComponentStatus(componentNames[i], initialStatus);
   }

}

// Get all the atabeam names out of the config file.
// Translate them into TscopeBeam enum values and store
// them in the ataBeamStatusIndices_ for use by the status
// display methods.  Log any errors (e.g., bad beam names)

void SiteInternal::loadAtaBeamStatusIndices()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(configMutex_);

   Assert(expectedNssComponentsTree_);
   vector<string> ataBeamNames(expectedNssComponentsTree_->getAtaBeams());
   ataBeamStatusIndices_.clear();

   // convert each ata beam name to a valid index into the status array
   for (vector<string>::iterator it = ataBeamNames.begin();
        it != ataBeamNames.end(); ++it)
   {
      const string & ataName = *it;



      TscopeBeam beamIndex = SseTscopeMsg::nameToBeam(ataName);
      if (beamIndex == TSCOPE_INVALID_BEAM)
      {
	 stringstream strm;
	 strm << "Site:: invalid ata beam name in "
	      << "expected components config file: " 
	      << ataName << endl;

	 // log may not be visible in UI when this is printed, so
	 // send to cerr as well
	 cerr << strm.str() << endl;

	 SseMessage::log(MsgSender,
                         NSS_NO_ACTIVITY_ID, SSE_MSG_FILE_ERROR,
                         SEVERITY_WARNING, strm.str(),
                         __FILE__, __LINE__);
      }
      else 
      {
	 ataBeamStatusIndices_.push_back(beamIndex); 
      }
      
   }
}

vector<TscopeBeam> Site::getAssignedAtaBeamStatusIndices()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(internal_->configMutex_);

   return internal_->ataBeamStatusIndices_;
}
