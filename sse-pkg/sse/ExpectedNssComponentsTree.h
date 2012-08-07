/*******************************************************************************

 File:    ExpectedNssComponentsTree.h
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


#ifndef ExpectedNssComponentsTree_H
#define ExpectedNssComponentsTree_H

#include <vector>
#include <string>
#include <iostream>

using std::vector;
using std::string;
using std::ostream;

// Contains a tree of Nss component names
// which are loaded from a configuration file.
// (See configuration file description and example below).
// The tree shows how the various components are related.

// The component names can be asked for by
// type, or by the name of the  parent component they are
// attached to (ie, the datapath).
// The component types in descending order are:
// Site, Ifc, Beam, Dx.

class ExpectedNssComponentsTreeInternal;

class ExpectedNssComponentsTree
{
 public:
    // load the information from a config file.
    ExpectedNssComponentsTree(const string &configFilename, ostream &errorStrm = std::cerr);

    // load the information from a vector<string> version 
    // of the config file
    ExpectedNssComponentsTree(vector<string> &configVector, ostream &errorStrm = std::cerr);

    // Dummy default version
    ExpectedNssComponentsTree(ostream &errorStrm = std::cerr);

    virtual ~ExpectedNssComponentsTree();

    // get all components, regardless of type
    vector<string> getAllComponents();

    // get all the components of a type
    vector<string> getSites();
    vector<string> getIfcs();
    vector<string> getBeams();
    vector<string> getDxs();
    vector<string> getZxs();
    vector<string> getChans();
    vector<string> getAtaBeams();

    // get the components "under" a given component
    vector<string> getBeamsForIfc(const string &ifc);
    vector<string> getDxsForBeam(const string &beam);
    vector<string> getZxsForBeam(const string &beam);
    vector<string> getChansForBeam(const string &beam);
    vector<string> getDxsForIfc(const string &ifc);
    vector<string> getZxsForIfc(const string &ifc);

    // get all components of a type for a site
    vector<string> getIfcsForSite(const string &site);
    vector<string> getDxsForSite(const string &site);
    vector<string> getZxsForSite(const string &site);
    vector<string> getChansForSite(const string &site);

    string getBeamForDx(const string & dx);
    string getBeamForZx(const string & dx);
    string getBeamForChan(const string & chan);
    string getChanForDx(const string & dx);
    string getChanForZx(const string & dx);
    string getIfcForDx(const string &dx);
    string getIfcForZx(const string &dx);

    vector<string> getAtaBeamsForBeam(const string &beam);

    // output all the component information
    friend ostream& operator << (ostream &strm, 
				 const ExpectedNssComponentsTree &tree);

 private:
    // Disable copy construction & assignment.
    // Don't define these.
    ExpectedNssComponentsTree(const ExpectedNssComponentsTree& rhs);
    ExpectedNssComponentsTree& operator=(const ExpectedNssComponentsTree& rhs);
    ExpectedNssComponentsTreeInternal *internal_;

};

/* 

Config file format:

The first line of the config file must be a standard header 
that describes the configuration file type and version. 
The header must contain the following text:

  sonata expected components vX.X

This header is followed by component entries, one per line,
which must follow one of these patterns:

  Site <site name> IfcList <ifc name> [... <ifc name>]
  Ifc <ifc name> BeamList <beam name> [... <beam name>]
  Beam <beam name> DxList <dx name> [... <dx name>]
  BeamToAtaBeams <beam name> <ataBeamName> [... <ataBeamName>]

For the Site, Ifc, and Beam patterns,
each line consists of these parts:

  The first field is the component type, which must be
  one of: "Site", "Ifc", or "Beam", 

  The second field is the component name.

  The third field is the "child component list type".  It must be
  one of "IfcList", "BeamList", or "DxList".  It must
  also be the appropriate type for its parent, as shown above.

  Any remaining fields on the line are the names of the children of this component.
  There must be at least one.   Its expected that each child component name 
  will have a subsequent entry of its own, listing its own children.
  (The exception is Dxs, which have no separate entries).  In this way the
  connections between the components (i.e., the data paths) are indicated.

  For BeamToAtaBeams, the line lists the ata beams associated with the
  given (Prelude) beam.

Comments start with #, which may appear anywhere in the line.
All text after the # is ignored.

Comments and blank lines may appear anywhere in the file except
the first line.

*/

// Example config file:

/*
nss expected components v1.6

# NSS Components Configuration File
# This is a comment.

Site Main IfcList ifc0 ifc1
Ifc ifc0 BeamList beam0
Beam beam0 DxList dx0 dx1 dx2
Ifc ifc1 BeamList beam1
Beam beam1 DxList dx3 dx4
BeamToAtaBeams beam0 beamxa1 beamya1
*/

#endif // ExpectedNssComponentsTree_H
