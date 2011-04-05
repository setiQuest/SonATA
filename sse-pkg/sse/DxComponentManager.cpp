/*******************************************************************************

 File:    DxComponentManager.cpp
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



// DxProxy subclassing of NssComponentManager template.
// Also private supporting routines.

#include <ace/OS.h>
#include "DxComponentManager.h" 
#include "sseDxInterface.h"
#include "DxProxy.h"
#include "SignalMask.h"
#include "ExpectedNssComponentsTree.h"
#include "IfcNames.h"
#include "SseMessage.h"
#include "MsgSender.h"

static const string RcvrBirdieMaskFilename("rcvrBirdieMask.tcl");
static const string BirdieMaskFilenameIfc1("birdieMask.tcl");
static const string BirdieMaskFilenameIfc2("birdieMask.tcl");
static const string BirdieMaskFilenameIfc3("birdieMask.tcl");


static void sendConfigureDx(DxProxy *proxy, const string &archiverHostname,
			     const string &archiverPort, int verboseLevel); 

class BirdieMask : public DxSignalMask 
{
public:
    BirdieMask(const string &maskFilename, const string &maskType, 
		int verboseLevel) :
	DxSignalMask(maskFilename, maskType, verboseLevel)
    {}

protected:
    virtual void forwardMaskToDx(DxProxy *proxy, 
			     const FrequencyMaskHeader &maskHeader,
			     const FrequencyBand freqBandArray[])
    {
	proxy->sendBirdieMask(maskHeader, freqBandArray);
    }

};

class RcvrBirdieMask : public DxSignalMask 
{
public:
    RcvrBirdieMask(const string &maskFilename, const string &maskType, 
		int verboseLevel) :
	DxSignalMask(maskFilename, maskType, verboseLevel)
    {}

protected:
    virtual void forwardMaskToDx(DxProxy *proxy, 
			     const FrequencyMaskHeader &maskHeader,
			     const FrequencyBand freqBandArray[])
    {
	proxy->sendRcvrBirdieMask(maskHeader, freqBandArray);
    }

};


class RecentRfiMask : public DxSignalMask 
{
public:
    RecentRfiMask(const string &maskFilename, const string &maskType, 
		int verboseLevel) :
	DxSignalMask(maskFilename, maskType, verboseLevel)
    {}

protected:
    virtual void forwardMaskToDx(DxProxy *proxy, 
			     const FrequencyMaskHeader &maskHeader,
			     const FrequencyBand freqBandArray[])
    {
	RecentRfiMaskHeader recentRfiHeader;
	recentRfiHeader.numberOfFreqBands = maskHeader.numberOfFreqBands;
	recentRfiHeader.bandCovered = maskHeader.bandCovered;
	recentRfiHeader.excludedTargetId = 0;  // more use of this TBD

	proxy->sendRecentRfiMask(recentRfiHeader, freqBandArray);
    }

};

class TestSignalMask : public DxSignalMask 
{
public:
    TestSignalMask(const string &maskFilename, const string &maskType, 
		int verboseLevel) :
	DxSignalMask(maskFilename, maskType, verboseLevel)
    {}

protected:
    virtual void forwardMaskToDx(DxProxy *proxy, 
			     const FrequencyMaskHeader &maskHeader,
			     const FrequencyBand freqBandArray[])
    {
	proxy->sendTestSignalMask(maskHeader, freqBandArray);
    }

};


void DxComponentManager::setExpectedNssComponentsTree(
    ExpectedNssComponentsTree *tree)
{
    expectedNssComponentsTree_ = tree;    
}


string DxComponentManager::getIfcNameForDx(const string &dxName)
{
    const string defaultIfcName(Ifc1Name);

    Assert(expectedNssComponentsTree_);
    string ifcName(expectedNssComponentsTree_->getIfcForDx(dxName));
    if (ifcName == "")
    {
       ifcName = defaultIfcName;
    }

    return ifcName;
}

// This is a hook called by the NssComponentManager when
// previously requested dx intrinsics info
// arrives from the physical dx.   The base class 'receiveIntrinsics'
// method handles version checking etc. so that this
// can concentrate on dx specific actions.


void DxComponentManager::
additionalReceiveIntrinsicsProcessing(DxProxy *proxy)
{
    SseArchive::SystemLog()
	<< "Received intrinsics from " << proxy->getName() << endl
	<< proxy->getIntrinsics() 
	<< "Dx Bandwidth in MHz: " << proxy->getBandwidthInMHz() << endl;

    // Figure out which if-chain/network this dx is on,
    // to determine which dx archiver it needs to attach to
    // and which birdie mask to send.  

    string archiverHostname = archiver1Hostname_;
    string archiverPort = archiver1Port_;
    string birdieMaskFilename = BirdieMaskFilenameIfc1;

    if (getIfcNameForDx(proxy->getName()) == Ifc1Name)
    {
       // use the defaults
    }
    else if (getIfcNameForDx(proxy->getName()) == Ifc2Name)
    {
	archiverHostname = archiver2Hostname_;
	archiverPort = archiver2Port_;
	birdieMaskFilename = BirdieMaskFilenameIfc2;
    }
    else if (getIfcNameForDx(proxy->getName()) == Ifc3Name)
    {
	archiverHostname = archiver3Hostname_;
	archiverPort = archiver3Port_;
	birdieMaskFilename = BirdieMaskFilenameIfc3;
    }
    else
    {
       AssertMsg(0, "invalid ifc name for dx");
    }
       
    VERBOSE1(verboseLevel_,
             "DxProxy subclass override: received intrinsics for " 
	     << proxyTypename_ << " " << proxy << endl
	     << "archiver host: " << archiverHostname << endl;
	);


    // configure the dx, based in part on intrinsics
    sendConfigureDx(proxy, archiverHostname, archiverPort,
		     verboseLevel_);

    // only need send these masks if out of date.
    // TBD: add timestamp comparison to info in intrinsics
    PermRfiMask permRfiMask(permRfiMaskFilename_, "permRfiMask", verboseLevel_);
    permRfiMask.sendMaskToDx(proxy);


    RcvrBirdieMask rcvrBirdieMask(RcvrBirdieMaskFilename, "rcvrBirdieMask",
				  verboseLevel_);
    rcvrBirdieMask.sendMaskToDx(proxy);


    BirdieMask birdieMask(birdieMaskFilename, "birdieMask", verboseLevel_);
    birdieMask.sendMaskToDx(proxy);


    // debug send of these masks.  move to activity setup?

    //RecentRfiMask recentRfiMask("recentMask.tcl", "recentRfiMask", verboseLevel_);
    //recentRfiMask.sendMaskToDx(proxy);

    //TestSignalMask testSignalMask("testMask.tcl", "testSignalMask", verboseLevel_);
    //testSignalMask.sendMaskToDx(proxy);

}

// ------- local static methods ----------------
// ---------------------------------------------


// set up dx configuration information, based in part
// on the intrinsics.
static void sendConfigureDx(DxProxy *proxy, const string &archiverHostname,
			     const string &archiverPort, int verboseLevel)
{

    // getIntrinsics? 

    // dummy config info
    DxConfiguration config;

    config.site=SITE_ID_ATA; // TBD should get from act parameters
    config.dxId=0;
    config.a2dClockrate=100;

    // set archiver hostname & port
    SseUtil::strMaxCpy(config.archiverHostname, archiverHostname.c_str(),
		       MAX_TEXT_STRING);

    int port;
    try {
	port = 
	    SseUtil::strToInt(archiverPort);  // this will throw on error

    } catch (...) {
	stringstream strm;
	strm << "Invalid dx archiver port: "
			       << archiverPort
			       <<endl;
	SseMessage::log(MsgSender,
                        0, SSE_MSG_INVALID_PORT,
                        SEVERITY_ERROR, strm.str().c_str(),
                        __FILE__, __LINE__);
	return;
    }

    config.archiverPort = port;

    VERBOSE2(verboseLevel,
	     "DxComponentManager: sending configuration to dx" << endl;);
    proxy->configureDx(config);

}









