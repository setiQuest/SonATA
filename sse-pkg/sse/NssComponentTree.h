/*******************************************************************************

 File:    NssComponentTree.h
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


#ifndef NssComponentTree_H
#define NssComponentTree_H

#include <ChannelizerList.h>
#include <DxList.h>
#include <IfcList.h>
#include <TscopeList.h>
#include <TestSigList.h>
#include <vector>
#include <string>

// This class holds components, represented by their proxies.

class NssComponentTreeInternal;
class ExpectedNssComponentsTree;

class NssComponentTree
{
 public:
    NssComponentTree(const DxList &dxList,
                     const DxList &zxList,
		     const ChannelizerList & chanList,
		     const IfcList &ifcList,
		     const TscopeList &tscopeList,
		     const TestSigList &testSigList,
		     ExpectedNssComponentsTree *expectedNssComponentsTree);

    virtual ~NssComponentTree();

    DxList & getDxs();
    DxList & getZxs();
    DxList & getDxsForIfc(IfcProxy *ifcProxy);
    DxList & getZxsForIfc(IfcProxy *ifcProxy);

    ChannelizerList & getChans();
    IfcList & getIfcs();
    TscopeList & getTscopes();
    TestSigList & getTestSigs();

    IfcList getIfcsForBeams(std::vector<std::string> beamNames);
    DxList getDxsForBeams(std::vector<std::string> beamNames);
    DxList getZxsForBeams(std::vector<std::string> beamNames);
    ChannelizerList getChansForBeams(std::vector<std::string> beamNames);

    ExpectedNssComponentsTree *getExpectedNssComponentsTree();

 private:

    NssComponentTreeInternal *internal_;

    // Disable copy construction & assignment.
    // Don't define these.
    NssComponentTree(const NssComponentTree& rhs);
    NssComponentTree& operator=(const NssComponentTree& rhs);

};

#endif // NssComponentTree_H
