/*******************************************************************************

 File:    NssComponentManager.h
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


#ifndef NssComponentManager_H
#define NssComponentManager_H

#include "ace/Synch.h"

#ifndef SSECOMMUTIL_VERBOSE_OVERRIDE
#include "Verbose.h"
#endif

#include "Publisher.h"
#include "Assert.h"
#include <typeinfo>
#include <list>
#include <map>
#include <sstream>
#include <iostream>
#include "sseInterface.h"
#include "SseMessage.h"

using std::string;
using std::stringstream;
using std::endl;
using std::cout;
using std::cerr;

// This class handles storage of Nss components.
// Components are represented by proxy pointers.

class Subscriber;

template<typename Tproxy>

class NssComponentManager
{
 public:

    typedef std::list<Tproxy *> TproxyList;
    typedef typename TproxyList::iterator TproxyListIterator;

    enum DuplicateNameHandling { RejectNewProxyWithDuplicateName,
				 DiscardOldProxyWithDuplicateName };

    NssComponentManager(Subscriber *subscriber, 
			DuplicateNameHandling duplicateNameHandling =
			RejectNewProxyWithDuplicateName);
    virtual ~NssComponentManager();

    virtual void setManagerName(const string &name);
    virtual string getManagerName();

    virtual int getNumberOfProxies();
    virtual string getNamesOfProxies();

    virtual void registerProxy(Tproxy *proxy);
    virtual void unregisterProxy(Tproxy *proxy);

    virtual void getProxyList(TproxyList *proxyList);  
    virtual void allocateProxyList(TproxyList *proxyList);
    virtual void releaseProxyList(TproxyList *proxyList);

    virtual void receiveIntrinsics(Tproxy *proxy);

    virtual void notifyStatusChanged(Tproxy *proxy);
    virtual void processNssMessage(Tproxy *proxy, NssMessage &nssMessage);
    virtual void processNssMessage(Tproxy *proxy, NssMessage &nssMessage,
			   int activityId, const string &hostname);

    virtual void printProxyList();
    //void printDetailedProxyList();

    virtual void setVerbose(int verboseLevel);
    virtual int getVerbose() {
	return verboseLevel_;
    }


 protected:
    int verboseLevel_;
    string proxyTypename_;
    bool proxyNameAlreadyRegistered(Tproxy *proxy);

    virtual void additionalReceiveIntrinsicsProcessing(Tproxy *proxy);

    virtual void logErrorMsg(const string &msgText);
    virtual void logInfoMsg(const string &msgText);

    virtual void rejectProxy(Tproxy *proxy);

    virtual void discardOldProxyWithDuplicateName(Tproxy *newProxy);
    virtual void rejectNewProxyWithDuplicateName(Tproxy *newProxy);


 private:
    // Disable copy construction & assignment.
    // Don't define these.
    NssComponentManager(const NssComponentManager& rhs);
    NssComponentManager& operator=(const NssComponentManager& rhs);

    void storeProxy(Tproxy *proxy);
    void removeProxy(Tproxy *proxy);


    enum ComponentAllocStatus { COMPONENT_FREE, COMPONENT_ALLOCATED };
    typedef std::map<Tproxy *, ComponentAllocStatus> ComponentMap;
    typedef typename ComponentMap::iterator ComponentMapIterator;
    ComponentMap componentMap_;  
    ACE_Recursive_Thread_Mutex objectMutex_;
    ACE_Recursive_Thread_Mutex componentMapMutex_;
    Subscriber *subscriber_;
    DuplicateNameHandling duplicateNameHandling_;

    Publisher publisher_;
    string managerName_;



};

//-------------------------------------------------------------------

// constructor
template<typename Tproxy>
NssComponentManager<Tproxy>::
NssComponentManager(Subscriber *subscriber, DuplicateNameHandling duplicateNameHandling)
    :
   verboseLevel_(0),
   subscriber_(subscriber),
   duplicateNameHandling_(duplicateNameHandling)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    proxyTypename_ = typeid(Tproxy).name();

    publisher_.attach(subscriber_);
}

// destructor
template<typename Tproxy>
NssComponentManager<Tproxy>::~NssComponentManager()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    publisher_.detach(subscriber_);
}

template<typename Tproxy>
void NssComponentManager<Tproxy>::
setManagerName(const string &name)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);
    managerName_ = name;
}

template<typename Tproxy>
string NssComponentManager<Tproxy>::
getManagerName()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);
    return managerName_;
}



// store the proxy for the component, marking it as 'free'
//
template<typename Tproxy>
void NssComponentManager<Tproxy>::
storeProxy(Tproxy *proxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    // store the proxy pointer, marking it as 'free'
    if (!componentMap_.insert(std::make_pair(proxy,COMPONENT_FREE)).second)
    {
	cerr << "Error: Failure inserting proxy! " << proxy << endl;
	Assert(0);
    }
}


// register the proxy
//
template<typename Tproxy>
void NssComponentManager<Tproxy>::
registerProxy(Tproxy *proxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(verboseLevel_,
             "registering " << proxyTypename_ << " " << proxy 
	     << " " << proxy->getName() << endl;);

    stringstream infoStrm;
    infoStrm << managerName_ << ": registering " <<  proxyTypename_
	<< " Host: " << proxy->getRemoteHostname() 
	<< " Name: " << proxy->getName() << endl;

    logInfoMsg(infoStrm.str());

    // debug
    //printProxyList();

    storeProxy(proxy);

    // debug
    //printProxyList();

    // TBD make this work with all proxy types
    proxy->setVerboseLevel(verboseLevel_);
    
    // get info about the component
    proxy->requestIntrinsics();

    // notify the subscriber(s) that the state has changed
    publisher_.notify();

}

template<typename Tproxy>
bool NssComponentManager<Tproxy>::
proxyNameAlreadyRegistered(Tproxy *checkingProxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    // look up the name of the checkingProxy in the list, 
    // return true if the name matches
    // any proxy other than the one checking

    string checkingProxyName(checkingProxy->getName());
    ComponentMapIterator it;
    for (it = componentMap_.begin(); it != componentMap_.end(); ++it)
    {
	Tproxy *proxy = it->first;
	if (proxy != checkingProxy &&
	    proxy->getName() == checkingProxyName)
	{
	    return true;
	}
    }

    return false;

}


template<typename Tproxy>
void NssComponentManager<Tproxy>::
removeProxy(Tproxy *proxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    // make sure proxy is found  -- error if not there!
    Assert (componentMap_.find(proxy) != componentMap_.end()); 

    //debug
    //printProxyList();

    // take proxy pointer off the list
    componentMap_.erase(proxy);  

    //debug
    //printProxyList();
}


template<typename Tproxy>
int NssComponentManager<Tproxy>::
getNumberOfProxies()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    return componentMap_.size(); 
}

// get the names of all the proxies, separated by white space
template<typename Tproxy>
string NssComponentManager<Tproxy>::
getNamesOfProxies()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    stringstream strm;

    ComponentMapIterator p;
    for (p = componentMap_.begin(); p != componentMap_.end(); ++p)
    {
	Tproxy *proxy = p->first;
	strm << proxy->getName() << " ";
    }

    return strm.str();
}

// Unregister components when they go away.
// Need to notify any active proxy users! (mechanism tbd)
//
template<typename Tproxy>
void NssComponentManager<Tproxy>::
unregisterProxy(Tproxy *proxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(verboseLevel_, "unregistering proxy " << proxyTypename_ 
	     << " "<< proxy << " " <<  proxy->getName() << endl;);

    stringstream infoStrm;

    infoStrm << managerName_ << ": unregistering " << proxyTypename_
	<< " Host: " << proxy->getRemoteHostname() 
	<< " Name: " << proxy->getName() << endl;

    logInfoMsg(infoStrm.str());

    removeProxy(proxy);

    // notify the subscriber(s) that the state has changed
    publisher_.notify();

}

template<typename Tproxy> 
void NssComponentManager<Tproxy>::
setVerbose(int verboseLevel)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    verboseLevel_ = verboseLevel;

    // set the verbose level on all the proxies
    ComponentMapIterator p;
    for (p = componentMap_.begin(); p != componentMap_.end(); ++p)
    {
	Tproxy *proxy = p->first;
	proxy->setVerboseLevel(verboseLevel_);
    }

}


// Return all the stored components, regardless of their
// allocation status.  Components are put into the list supplied
// by the caller.

template<typename Tproxy> 
void NssComponentManager<Tproxy>::
getProxyList(TproxyList *proxyList)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    //VERBOSE2(verboseLevel_, "getProxyList " <<  proxyTypename_  << endl;);
    ComponentMapIterator p;
    for (p = componentMap_.begin(); p != componentMap_.end(); ++p)
    {
	    proxyList->push_back(p->first);
    }

}

// Return a list of component proxies that meet some tbd requirements.
// For now, just return all that we have that are marked as 'free'.
// Caller passes in an empty list, which we fill 
// with copies of the proxy pointers.
// Proxies are marked as allocated.
//
template<typename Tproxy> 
void NssComponentManager<Tproxy>::
allocateProxyList(TproxyList *proxyList)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    VERBOSE2(verboseLevel_, "allocateProxyList " <<  proxyTypename_ << endl;);

    ComponentMapIterator p;
    for (p = componentMap_.begin(); p != componentMap_.end(); ++p)
    {
	// allocate the free components
	if (p->second == COMPONENT_FREE)
	{
	    // cout << "proxy addr: " << p->first << endl;
	    proxyList->push_back(p->first);
	    p->second = COMPONENT_ALLOCATED;   // mark them as allocated
	}
    }

}

// Release previously allocated component proxies so that
// they can be reused.
//
template<typename Tproxy> 
void NssComponentManager<Tproxy>::
releaseProxyList(TproxyList *proxyList)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    VERBOSE2(verboseLevel_, "releaseProxyList " <<  proxyTypename_ << endl;);

    // Go through the proxyList.  Look up each proxy
    // in the local component map and mark it as a free
    // component.
    // Error if proxy not there or not marked as allocated.

    TproxyListIterator p;
    for (p = proxyList->begin(); p != proxyList->end(); ++p)
    {
	ComponentMapIterator itMap;
	itMap = componentMap_.find(*p);

	// A component may disappear at any time, so it's not
	// a fatal error if it's not found.
	// More error handling TBD.

	if (itMap == componentMap_.end())
	{
	    stringstream msgStrm;
	    msgStrm << "NssComponentManager::releaseProxyList "
		<< proxyTypename_ << " Proxy not found" << endl;

	    SseMessage::log(
               "Sse", NSS_NO_ACTIVITY_ID, SSE_MSG_PROXY_NOT_FOUND,
               SEVERITY_WARNING, msgStrm.str(),
               __FILE__, __LINE__);
	}
	else
	{
	    // found it, free it
	    Assert (itMap->second == COMPONENT_ALLOCATED); // component not allocated!
	    itMap->second = COMPONENT_FREE;

	    VERBOSE2(verboseLevel_,
		     "releasing proxy.  addr: " << *p << endl;);
	}
    }

}


// print out the stored proxies
//
template<typename Tproxy> 
void NssComponentManager<Tproxy>::
printProxyList()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

    const char *DeviceAllocStatusString[] =
    {
	"Free",
	"Allocated"
    };

    // test print the component proxy list
    cout << "------ " << proxyTypename_;   
    cout << " Component list: ----------" << endl;

    ComponentMapIterator p;
    for (p = componentMap_.begin(); p != componentMap_.end(); ++p)
    {
	Tproxy *proxy = p->first;
	cout << "name: " << proxy->getName() 
	     << "  host: " << proxy->getRemoteHostname()
	     << "  proxy-addr: " << proxy
 	     << "  alloc-state: " << DeviceAllocStatusString[p->second] << endl;
    }

    cout << "-----------------------------------" << endl;

}

// proxy status has changed.
template<typename Tproxy>
void NssComponentManager<Tproxy>::
notifyStatusChanged(Tproxy *proxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    // notify the subscriber(s) that the proxy status has changed
    // TBD: someday report which proxy has changed
    publisher_.notify();
}


template<typename Tproxy>
void NssComponentManager<Tproxy>::
processNssMessage(Tproxy *proxy, NssMessage &nssMessage)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(verboseLevel_,
             "Proxy message " << proxy->getName() << endl;);

    stringstream msgStrm;
    msgStrm << managerName_ << ": "
            << proxyTypename_ << " "
            << proxy->getName() << " " 
            << nssMessage;

    SseMessage::log(proxy->getName(),
                    NSS_NO_ACTIVITY_ID, SSE_MSG_INFO,
                    nssMessage.severity, msgStrm.str(),
                    __FILE__, __LINE__);
}

template<typename Tproxy>
void NssComponentManager<Tproxy>::
processNssMessage(Tproxy *proxy, NssMessage &nssMessage, int activityId,
                  const string &hostname)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(verboseLevel_,
             "Proxy message " << proxy->getName() << endl;);

    // TBD hostname appears to always be "unknown"
    // so just use the proxy name instead

    stringstream msgStrm;
    msgStrm << managerName_ << ": " 
            << proxyTypename_ 
            << " " << " Sender: "
            << proxy->getName() << " "
            << " Act: " << activityId << " "
            << nssMessage;
    
    SseMessage::log(
       hostname, activityId, SSE_MSG_INFO, nssMessage.severity,
       msgStrm.str(), __FILE__, __LINE__);
}


template<typename Tproxy>
void NssComponentManager<Tproxy>::logErrorMsg(const string &msgText)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    SseMessage::log("Unknown host", NSS_NO_ACTIVITY_ID,
                    SSE_MSG_UNKNOWN_ERROR, SEVERITY_WARNING,
                    msgText,
                    __FILE__, __LINE__);

    cerr << msgText;
    cerr.flush();
}


template<typename Tproxy>
void NssComponentManager<Tproxy>::
logInfoMsg(const string &msgText)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    cout << msgText;
    cout.flush();
}

template<typename Tproxy>
void NssComponentManager<Tproxy>::
rejectProxy(Tproxy *proxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    proxy->shutdown();
    proxy->resetSocket();
}

template<typename Tproxy>
void NssComponentManager<Tproxy>::
rejectNewProxyWithDuplicateName(Tproxy *newProxy)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);

   stringstream strm;
   
   strm << managerName_ << ": "  
	<< "Trying to register more than one " << proxyTypename_ 
	<< " with the same name: " << newProxy->getName()
	<< ", disconnecting new component." << endl;
   
   SseMessage::log(newProxy->getName(),
                   NSS_NO_ACTIVITY_ID, SSE_MSG_ALREADY_ATTACH,
                   SEVERITY_ERROR, strm.str(),
                   __FILE__, __LINE__);
   
   rejectProxy(newProxy);

}

template<typename Tproxy>
void NssComponentManager<Tproxy>::
discardOldProxyWithDuplicateName(Tproxy *newProxy)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(componentMapMutex_);
   
   stringstream strm;
   
   strm << managerName_ << ": "  
	<< "Trying to register more than one " << proxyTypename_ 
	<< " with the same name: " << newProxy->getName()
	<< ", disconnecting old component." << endl;
   
   SseMessage::log(newProxy->getName(),
                   NSS_NO_ACTIVITY_ID, SSE_MSG_ALREADY_ATTACH,
                   SEVERITY_ERROR, strm.str(),
                   __FILE__, __LINE__);
   
   // reject the old proxy with the same name as this new one
   string name(newProxy->getName());
   ComponentMapIterator it;
   for (it = componentMap_.begin(); it != componentMap_.end(); ++it)
   {
      Tproxy *oldProxy = it->first;
      if (oldProxy != newProxy &&
	  oldProxy->getName() == name)
      {
	 rejectProxy(oldProxy);
      }
   }
   
}



// received proxy intrinsics info -- act on it
// generic version -- watch for specializations
template<typename Tproxy>
void NssComponentManager<Tproxy>::
receiveIntrinsics(Tproxy *proxy)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(objectMutex_);

    VERBOSE1(verboseLevel_,
             "received intrinsics for " << proxyTypename_ << " " << proxy << endl;);
    if (!proxy->validInterfaceVersion())
    {
	// invalid interface, disconnect it
	stringstream strm;

	strm << managerName_ 
	     << ": Invalid interface version for " <<  proxyTypename_
	     << ", disconnecting component: " << proxy->getName() << endl;
	
	SseMessage::log(proxy->getName(),
                        NSS_NO_ACTIVITY_ID, SSE_MSG_INTERFACE_MISMATCH,
                        SEVERITY_ERROR, strm.str(),
                        __FILE__, __LINE__);
        
	rejectProxy(proxy);

	return;
    }

    // see if a proxy with this name is already on the list
    if (proxyNameAlreadyRegistered(proxy))
    {
       if (duplicateNameHandling_ == RejectNewProxyWithDuplicateName)
       {
	  rejectNewProxyWithDuplicateName(proxy);
	  
	  return;
       }
       else 
       {
	  discardOldProxyWithDuplicateName(proxy);
       }
       
    }

    stringstream strm;
    strm << managerName_ << ": Received intrinsics from " 
	 << proxy->getName() << endl;
    if ( strncmp( proxy->getName().c_str(), "chan", 4) == 0)
	strm << proxy->getIntrinsics();
    logInfoMsg(strm.str());

    proxy->requestStatusUpdate();

    // hook for subclasses to do anything else they need to do
    additionalReceiveIntrinsicsProcessing(proxy);

    // notify the subscriber(s) that the state has changed
    publisher_.notify();

}

template<typename Tproxy>
void NssComponentManager<Tproxy>::
additionalReceiveIntrinsicsProcessing(Tproxy *proxy)
{
    // NOP - let subclasses override
}


#endif // NssComponentManager_H
