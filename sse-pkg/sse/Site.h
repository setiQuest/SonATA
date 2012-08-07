/*******************************************************************************

 File:    Site.h
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


#ifndef _SITE_H
#define _SITE_H

#include "DebugLog.h"  // keep this early in the headers for VERBOSE macros
#include "NssComponentManager.h"
#include "NssComponentTree.h"
#include "sseTscopeInterface.h"
#include "Timeout.h"

class DxProxy;
class DxArchiverProxy;
class ChannelizerProxy;
class IfcProxy;
class TscopeProxy;
class TestSigProxy;
class ComponentControlProxy;
class SiteInternal;
class ExpectedNssComponentsTree;

class Site {

public:
    Site(const string &dxPort, 
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
	 bool noUi); 

    Site(vector<string> &expectedNssComponentsConfigVector);

   ~Site();

    NssComponentManager<DxProxy> *dxManager();
    NssComponentManager<DxProxy> *zxManager();
    NssComponentManager<DxArchiverProxy> *dxArchiverManager();
    NssComponentManager<ChannelizerProxy> *channelizerManager();
    NssComponentManager<IfcProxy> *ifcManager();
    NssComponentManager<TscopeProxy> *tscopeManager();
    NssComponentManager<TestSigProxy> *testSigManager();
    NssComponentManager<ComponentControlProxy> *componentControlManager();

    NssComponentTree *getAllComponents() const;

    void loadExpectedComponentsConfig(const string &expectedComponentsFilename);
    ExpectedNssComponentsTree *getExpectedNssComponentsTree();

    void updateSystemStatus();
    void repeatSystemStatus();
    void printSystemStatus(ostream &strm);
    void setVerbose(int verboseLevel);
    int getVerbose() const;

    vector<TscopeBeam> getAssignedAtaBeamStatusIndices();

 private:
    // Disable copy construction & assignment.
    // Don't define these.
    Site(const Site& rhs);
    Site& operator=(const Site& rhs);

    SiteInternal *internal_;

    Timeout<Site> systemStatusRepeatTimer_;

};



#endif // _site_h
