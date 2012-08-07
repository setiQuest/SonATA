/*******************************************************************************

 File:    ExpectedNssComponentsTree.cpp
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



#include "ExpectedNssComponentsTree.h" 
#include "SseUtil.h"
#include <algorithm>
#include "Assert.h"
#include <map>
#include <sstream>

using namespace std;

// Parse a config file containing information about nss
// component names and their children.  See .h file
// for config file format description.

static const string SiteType("Site");
static const string IfcType("Ifc");
static const string BeamType("Beam");
static const string DxType("Dx");
static const string ZxType("Zx");
static const string ChanType("Chan");
static const string ChannelizerType("Channelizer");
static const string NoneType("None");
static const string BeamToAtaBeamsKeyword("BeamToAtaBeams");
static const string ChanToBeamsKeyword("Channelizer");
static const string ConfigFileVersion("sonata expected components v1.0");

typedef multimap<string, string> BeamToAtaBeamsMultiMap;
typedef multimap<string, string> ChannelizerToBeamsMultiMap;

static void printErrorLine(ostream &strm, int linenum, const string & line)
{
   strm << "ExpectedNssComponents Error: Line " << linenum << ": " << line << endl;
}

//	This structure is initialized with the valid keywords
//	and is used to parse the expectedSonATAComponents.cfg file

struct ComponentTypeInfo
{
   string parentType_; 
   string type_;       
   string childListType_;
   string childType_;

   ComponentTypeInfo(const string &parentType,
		     const string &type,
		     const string &childListType,
		     const string &childType);


};

//	This structure is used to store the components
// 	specified in the expectedSonATAComponents.cfg file

ComponentTypeInfo::ComponentTypeInfo(const string &parentType,
				     const string &type,
				     const string &childListType,
				     const string &childType)
   :parentType_(parentType), type_(type),
    childListType_(childListType), childType_(childType)
{
}

// define a struct to hold info about a component
struct ComponentInfo
{
   string type_; 
   string name_;
   string siteName_;
   string parentName_;
   vector<string> childNames_;

};

struct ExpectedNssComponentsTreeInternal
{
   ExpectedNssComponentsTreeInternal(vector<string> &configLines, ostream & errorStrm);

   void parseConfigLines();
   void verifyThatChildListComponentsExist();
   void verifyComponentParents();

   void verifyAntLists();

   void checkAntListSubset(
      const string &subsetListName, vector<string> & subsetList,
      const string & masterListName, vector<string> & masterList);

   vector<string> prepareAntList(const string & antListKeyword);

   void turnDxsIntoSeparateComponents();

   void addComponent(const string &line, int linenum, vector<string> &tokens);
   bool verifyChildNotOnChildList(ComponentInfo &ci, const string &childName,
				  int linenum, const string &line);
   bool validComponentType(const string &componentType);
   bool validChildListType(const string &childListType, const string &componentType);
   string getChildType(const string &parentType);

   void printRawConfigLines(ostream &strm);
   void printComponents(ostream &strm);
   void printBeamToAtaBeamsAssociations(ostream &strm);
   void printChanToBeamsAssociations(ostream &strm);

   vector<string> findComponentNamesByType(const string &componentType);
   vector<string> findComponentNamesByTypeAndSite(
      const string &componentType, const string &siteName);
   vector<string> findComponentChildrenNames(const string &componentType,
					     const string &componentName);
   vector<string> findComponentChildrenNames(const string &componentType,
					     const string &componentName,
					     const string &childType);
   vector<string>getAllComponentNames();
   vector<string>findChannelizerNames();
   vector<string>findChannelizerNames( const string &beamname);

   ComponentInfo *findComponentInfoByName(const string &name);
   ComponentTypeInfo *getComponentTypeInfo(const string &componentType);
   void printComponentTypes(ostream &strm);
   void assignSiteToAllComponents();
   void assignSiteToComponentAndItsChildren(const string &siteName, 
					    const string &componentName);

   void addBeamToAtaBeamsAssociation(
      const string &line, int linenum, vector<string> &tokens);

   void addChanToBeamsAssociation(
      const string &line, int linenum, vector<string> &tokens);

   string getParent(const string &componentName);

   // data
   vector<string> rawConfigLines_;  
   vector<ComponentInfo> components_; 
   vector<ComponentTypeInfo> componentTypeInfo_;
   BeamToAtaBeamsMultiMap beamToAtaBeamsMap_;
   ChannelizerToBeamsMultiMap chanToBeamsMap_;
   ostream & errorStrm_;
};

ExpectedNssComponentsTreeInternal::ExpectedNssComponentsTreeInternal(vector<string> &configLines,
								     ostream &errorStrm)
   :rawConfigLines_(configLines),
    errorStrm_(errorStrm)
{
   // parentType, type, childListType, childType
   componentTypeInfo_.push_back(ComponentTypeInfo(NoneType, SiteType, 
						  "IfcList", IfcType));

   componentTypeInfo_.push_back(ComponentTypeInfo(SiteType, IfcType, 
						  "BeamList", BeamType));

   componentTypeInfo_.push_back(ComponentTypeInfo(IfcType, BeamType, 
						  "DxList", DxType));
   parseConfigLines();

    // add a new component type so that dxs can be turned into separate components
   componentTypeInfo_.push_back(ComponentTypeInfo(BeamType, DxType, NoneType, NoneType));
   turnDxsIntoSeparateComponents();


   verifyThatChildListComponentsExist();
   verifyComponentParents();
   assignSiteToAllComponents();
}


void ExpectedNssComponentsTreeInternal::printComponentTypes(ostream &strm)
{
   for (size_t i=0; i<componentTypeInfo_.size(); ++i)
   {
      strm << componentTypeInfo_[i].type_ << " ";
   } 
}

ComponentTypeInfo *ExpectedNssComponentsTreeInternal::getComponentTypeInfo(
   const string &componentType)
{
   for (size_t i=0; i<componentTypeInfo_.size(); ++i)
   {
      if (componentTypeInfo_[i].type_ == componentType)
      {
	 return &componentTypeInfo_[i];
      }
   } 

   AssertMsg(0, "component type not found");

}


bool  ExpectedNssComponentsTreeInternal::validComponentType(
   const string &componentType)
{
   for (size_t i=0; i<componentTypeInfo_.size(); ++i)
   {
      if (componentTypeInfo_[i].type_ == componentType)
      {
	 return true;
      }
   } 

   return false;
}

string ExpectedNssComponentsTreeInternal::getChildType(
   const string &parentType)
{
   for (size_t i=0; i<componentTypeInfo_.size(); ++i)
   {
      if (componentTypeInfo_[i].type_ == parentType)
      {
	 return componentTypeInfo_[i].childType_;
      }
   } 

   Assert(0);  // assume parent will always be found

   return "";
}


void ExpectedNssComponentsTreeInternal::turnDxsIntoSeparateComponents()
{
   vector<ComponentInfo> dxComponents;  // temp storage for new components
   vector<ComponentInfo> zxComponents;  // temp storage for new components

   // for each Beam component on the components list
   // turn its dx children into separate components
   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & ci = *i;
      if (ci.type_ == BeamType)	// Only dx and zx are BeamType
      {
	 // add a component for each child name
	 vector<string>::iterator childIt;
	 childIt=ci.childNames_.begin(); 
	    while (*childIt != "ZxList" && childIt < ci.childNames_.end()) 
            {
	    ComponentInfo dxCompInfo;
	    dxCompInfo.type_ = DxType;
	    dxCompInfo.name_ = *childIt;
	    dxCompInfo.parentName_ = ci.name_;

	    dxComponents.push_back(dxCompInfo);
	      ++childIt;
            }

	 if (*childIt == "ZxList")  
         {
	    vector<string>::iterator zxChildIt;
	    for (zxChildIt=childIt; zxChildIt < ci.childNames_.end(); 
	         zxChildIt++)
	    {
	       ComponentInfo zxCompInfo;

	       zxCompInfo.type_ = ZxType;
	       zxCompInfo.name_ = *zxChildIt;
	       zxCompInfo.parentName_ = ci.name_;
	       zxComponents.push_back(zxCompInfo);
	    }
         }

      }

   }


   // add all the new dx components to the original list
   for (i = dxComponents.begin(); i < dxComponents.end(); ++i)
   { 
      components_.push_back(*i);
   }

   // add all the new zx components to the original list
   for (i = zxComponents.begin(); i < zxComponents.end(); ++i)
   { 
      components_.push_back(*i);
   }
}


// verify that the childListType is legal
// ie, valid type, and compatible with the parent componentType
bool ExpectedNssComponentsTreeInternal::validChildListType(
   const string &childListType,  const string &componentType)
{
   for (size_t i=0; i<componentTypeInfo_.size(); ++i)
   {
      if (componentTypeInfo_[i].type_ == componentType &&
	  componentTypeInfo_[i].childListType_ == childListType)
      {
	 return true;
      }
   } 

   return false;
} 

void ExpectedNssComponentsTreeInternal::printRawConfigLines(ostream &strm)
{
   //strm << "ExpectedNssComponents:" << endl;
   vector<string>::iterator i;
   for (i = rawConfigLines_.begin(); i < rawConfigLines_.end(); ++i)
   { 
      strm << *i << endl;
   }
}

void ExpectedNssComponentsTreeInternal::printComponents(ostream &strm)
{
   strm << "ExpectedNssComponents:" << endl;
   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo &ci = *i;
      strm << "Type: " << ci.type_
	   << ", Name: " << ci.name_
	   << ", Site: " << ci.siteName_
	   << ", Parent: " << ci.parentName_
	   << ", Subcomponents: ";

      // print the child names
      vector<string>::iterator childIt;
      for (childIt=ci.childNames_.begin(); childIt < ci.childNames_.end(); 
	   ++childIt)
      {
	 strm << *childIt << " ";
      }
      strm << endl;
   }
}



void ExpectedNssComponentsTreeInternal::printBeamToAtaBeamsAssociations(
   ostream &strm)
{
   for (BeamToAtaBeamsMultiMap::iterator it = beamToAtaBeamsMap_.begin();
	it != beamToAtaBeamsMap_.end(); it++)
   {
      strm << it->first << " " << it->second << endl;
   }

}

void ExpectedNssComponentsTreeInternal::printChanToBeamsAssociations(
   ostream &strm)
{
   for (ChannelizerToBeamsMultiMap::iterator it = chanToBeamsMap_.begin();
	it != chanToBeamsMap_.end(); it++)
   {
      strm << it->first << " " << it->second << endl;
   }

}

// Pull out all of the information from the config file lines:
// Config file type & version number.
// Component names and their hierarchical relationships.
// Ignore comments and blank lines.

void ExpectedNssComponentsTreeInternal::parseConfigLines()
{
   // Read header on the first line.  Must match the expected information.

   const string &formatVersion = ConfigFileVersion;
   if (rawConfigLines_.empty() || 
       rawConfigLines_.front().find(formatVersion) == string::npos) 
   {
      errorStrm_ << "Error: ExpectedNssComponentsTree config file format/version mismatch." << endl;
      errorStrm_ << "First line should be: " << formatVersion << endl; 
      return;
   }

   // skip over first line, parse the rest
   vector<string>::iterator i = rawConfigLines_.begin() + 1; 
   int linenum = 2;
   for (; i < rawConfigLines_.end(); ++i, ++linenum)
   { 
      string line = *i;

      // DEBUG
      //cout << "parsing config line: " << linenum << ": " << line << endl;

      // erase any trailing comments
      string::size_type pos = line.find("#");
      if (pos != string::npos)
      {
	 line.erase(pos);
      }

      // break the line into tokens
      vector<string> tokens = SseUtil::tokenize(line, " ");
      if (tokens.size() > 0)  // ignore blank lines
      {
	 if (tokens[0] == BeamToAtaBeamsKeyword)
	 {
	    addBeamToAtaBeamsAssociation(line, linenum, tokens);
	 } 
         else if (tokens[0] == ChanToBeamsKeyword)
         {
	    addChanToBeamsAssociation(line, linenum, tokens);
         }
	 else
	 {
	    addComponent(line, linenum, tokens);
	 }
      }
   }
}

bool ExpectedNssComponentsTreeInternal::verifyChildNotOnChildList(
   ComponentInfo &ci,
   const string &childName,
   int linenum,
   const string &line)
{
   if ( childName == "ZxList") return true;
   if (find(ci.childNames_.begin(), ci.childNames_.end(),
	    childName) !=  ci.childNames_.end())
   {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Duplicate subcomponent: " << childName << endl;
      errorStrm_ << "Subcomponent already listed in component: " << ci.name_ << endl;
      return false;
   }
   return true;
}

// Add a component to the table.
// Tokens should be in this form:
// <Component Type> <Component Name> 
// <ChildComponentTypeList> <Child Name> [... <Child> Name]

void ExpectedNssComponentsTreeInternal::
addComponent(
   const string &line,      // original line, for error reporting
   int linenum,             // line number
   vector<string> &tokens)  // line broken into tokens
{
   Assert(tokens.size() > 0);
   string & componentType = tokens[0];

   if (!validComponentType(componentType))
   {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Invalid Component Type: " << "'" << componentType << "'" << endl;
      errorStrm_ << "Component type must be one of: "; 
      printComponentTypes(errorStrm_);
      return;
   }

   // make sure there are enough tokens for the minimum component description
   size_t minTokens = 4;
   if (tokens.size() < minTokens) {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Missing component information: " << line << endl;
      return;
   }

   // extract component info from lines of this form:
   // <component-type> <name> <subcomponent-List-type> <list of components...>
   // tbd verify keywords
   ComponentInfo compInfo;
   compInfo.type_ = componentType;
   compInfo.name_ = tokens[1];
   const string &childListType = tokens[2];

   // Make sure there's not already a component in the list by this name.
   if (findComponentInfoByName(compInfo.name_) != 0)
   {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Duplicate component: " << compInfo.name_ << endl;
      return;
   }
    
   if (!validChildListType(childListType, compInfo.type_))
   {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Invalid Subcomponent Type List: " << "'" 
		 << childListType << "'" << endl;

      errorStrm_ << "Subcomponent Type List must be '" 
		 << getComponentTypeInfo(compInfo.type_)->childListType_
		 << "' for component type '" << compInfo.type_ << "'." <<  endl;
      return;  
   }


   // pull off children from remaining tokens; verify them
   for (size_t i=3; i<tokens.size(); i++)
   {
      const string &childName = tokens[i];

      // make sure there isn't already a child by this name on this component
      if (verifyChildNotOnChildList(compInfo, childName, linenum, line))
      {
	 compInfo.childNames_.push_back(childName);
      }
      else {
	 return;  // duplicate child error
      }

      // make sure there isn't already a child by this name on any other component
      vector<ComponentInfo>::iterator i;
      for (i = components_.begin(); i < components_.end(); ++i)
      { 
	 ComponentInfo & ci = *i;

	 if (! verifyChildNotOnChildList(ci, childName, linenum, line))
	 {
	    return;  // duplicate child error
	 }
      }
   }

   // save the component
   components_.push_back(compInfo);

}


void ExpectedNssComponentsTreeInternal::assignSiteToComponentAndItsChildren(
   const string &siteName, const string &componentName)
{
   ComponentInfo * info = findComponentInfoByName(componentName);

   // Component info may not exist, because the component is a dx
   // which does not have its own entry, or because
   // the parse phase did not result in a completely error-free
   // component tree. 

   if (info)
   {
      // assign the site name
      info->siteName_ = siteName;

      // find the component's children, and recursively call this
      // method on them
      vector<string> &childNames = info->childNames_;
      for (size_t i = 0; i < childNames.size(); ++i)
      {
	 assignSiteToComponentAndItsChildren(siteName, childNames[i]);
      }
   }

}

void ExpectedNssComponentsTreeInternal::assignSiteToAllComponents()
{
   // recursively walk down the component tree for each site, 
   // marking that component as belonging to that site

   vector<string> siteNames = findComponentNamesByType(SiteType);
    
   for (size_t siteIndex = 0; siteIndex < siteNames.size(); ++siteIndex)
   {
      // To get started, each site component is marked as belonging
      // to itself.  This routine will then walk down the tree
      // working on each child component in turn.

      const string &siteName = siteNames[siteIndex];
      const string &componentName = siteName;
      assignSiteToComponentAndItsChildren(siteName, componentName);
   }

}


// Make sure all of the components have a parent, of the correct type,
// (except for Sites).
void ExpectedNssComponentsTreeInternal::verifyComponentParents()
{
   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & componentInfo = *i;
	
      if (componentInfo.type_ != SiteType)
      {
	 if (componentInfo.parentName_.size() == 0)
	 {
	    errorStrm_ << "ExpectedNssComponents Error: "
		       << "SubComponent '" << componentInfo.name_ 
		       << "' has no parent component." << endl;
	 }
      }

   }
}

void ExpectedNssComponentsTreeInternal::verifyThatChildListComponentsExist()
{
   // Make sure each child in a component's childNames list
   // has a component entry of it's own, and that the
   // entry is of the correct type.

   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & parentInfo = *i;

      vector<string>::iterator childNamesIt;
      for (childNamesIt = parentInfo.childNames_.begin(); 
	   childNamesIt < parentInfo.childNames_.end();
	   ++childNamesIt)
      { 
        
	 // check all the children
	 const string &childName = *childNamesIt;
       if (childName != "ZxList")
	{
	 ComponentInfo *childInfo = findComponentInfoByName(childName);
	 if (childInfo == 0)
	 {
	    // child component not found
	    errorStrm_ << "ExpectedNssComponents Warning: "
		       << "Subcomponent '" << childName 
		       << "' was listed under " 
		       << "component '" << parentInfo.name_
		       << "' but it has no entry of its own." << endl;
	 }
	 else 
	 {
	    // make sure child is of the right type for the parent type
	   if ( childInfo->type_ != ZxType)   // Don't woory about the Zxs
	   {
	    if (childInfo->type_ != getChildType(parentInfo.type_))
	    {
	       errorStrm_ << "ExpectedNssComponents Error: "
			  << "Subcomponent '" << childName 
			  << "' was listed under "
			  << "component '" << parentInfo.name_
			  << "' but its type '"<< childInfo->type_
			  << "' is incompatible with its parent's type." << endl;
	    }
	    else  // set the child's parent name
	    {
	       childInfo->parentName_ = parentInfo.name_;
	    }
	   }
	   else
		{
	          childInfo->parentName_ = parentInfo.name_;
		}
		    
	 }
	}
      }

   }

}
    
// Look up a component info entry.  Returns 0 if not found.
ComponentInfo *ExpectedNssComponentsTreeInternal::findComponentInfoByName(
   const string &name)
{
   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & componentInfo = *i;
      if (componentInfo.name_ == name)
      {
	 return &componentInfo;
      }
   }    
   return 0;
}



vector<string> ExpectedNssComponentsTreeInternal::findChannelizerNames()
{
   vector<string>names;

      for (ChannelizerToBeamsMultiMap::iterator it = chanToBeamsMap_.begin();
	   it !=  chanToBeamsMap_.end(); ++it)
      {
	 names.push_back(it->second);
      }

   return names;
}

vector<string> ExpectedNssComponentsTreeInternal::findChannelizerNames(const string & beamName)
{
   vector<string>names;

      for (ChannelizerToBeamsMultiMap::iterator it = chanToBeamsMap_.begin();
	   it !=  chanToBeamsMap_.end(); ++it)
      {
         if ( it->first == beamName ) names.push_back(it->second);
      }

   return names;
}


vector<string> ExpectedNssComponentsTreeInternal::findComponentNamesByType(
   const string &componentType)
{
   vector<string>names;

   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & ci = *i;
      if (ci.type_ == componentType)
      {
	 names.push_back(ci.name_);
      }
   }    

   return names;
}

vector<string> ExpectedNssComponentsTreeInternal::findComponentNamesByTypeAndSite(
   const string &componentType, const string &siteName)
{
   vector<string>names;

   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & ci = *i;
      if (ci.type_ == componentType && ci.siteName_ == siteName)
      {
	 names.push_back(ci.name_);
      }
   }    

   return names;
}


vector<string> ExpectedNssComponentsTreeInternal::findComponentChildrenNames(
   const string &componentType, const string &componentName)
{
   vector<string>names;

   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & ci = *i;
      if (ci.type_ == componentType)
      {
	 if (ci.name_ == componentName)
	 {
	    for (size_t i=0; i< ci.childNames_.size(); ++i)
	    {
	       names.push_back(ci.childNames_[i]);
	    }
	 }
      }
   }    

   return names;
}

vector<string> ExpectedNssComponentsTreeInternal::findComponentChildrenNames(
   const string &componentType, const string &componentName, const string &childType)
{
   vector<string>names;
   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & ci = *i;
      if (ci.type_ == componentType)
      {
	 if (ci.name_ == componentName)
	 {
           if (childType != ZxType )
           {
	    for (size_t i=0; i< ci.childNames_.size(); ++i)
	    {

             if ( ci.childNames_[i] != "ZxList")
             {
               ComponentInfo * child = findComponentInfoByName(ci.childNames_[i]);
               if( child->type_ == childType)
	       names.push_back(ci.childNames_[i]);
             }
	    }
	   }
           else  // we are looking for Zxs
           {
	    for (size_t i=0; i< ci.childNames_.size(); ++i)
	    {
             if ( ci.childNames_[i] == "ZxList")
             {
	       for (size_t y=i+1; y < ci.childNames_.size(); ++y)
               {
	       ComponentInfo * child = findComponentInfoByName(ci.childNames_[y]);
               if( child->type_ == childType)
	       names.push_back(ci.childNames_[y]);
               }
                break;
             }
	    }
           }
		
	 }
      }
   }    

   return names;
}


vector<string> ExpectedNssComponentsTreeInternal::getAllComponentNames()
{
   vector<string>names;

   vector<ComponentInfo>::iterator i;
   for (i = components_.begin(); i < components_.end(); ++i)
   { 
      ComponentInfo & ci = *i;
      names.push_back(ci.name_);
   }    

   return names;
}

// Add a beam-to-ata-beams association
// Tokens should be in this form:
// <BeamToAtaBeamsKeyword> <beam name> <ata beam name> [<ata beam name> ...]

void ExpectedNssComponentsTreeInternal::
addBeamToAtaBeamsAssociation(
   const string &line,      // original line, for error reporting
   int linenum,             // line number
   vector<string> &tokens)  // line broken into tokens
{
   Assert(tokens.size() > 0);
   Assert(tokens[0] == BeamToAtaBeamsKeyword);

   // make sure there are the correct number of tokens
   size_t minExpectedNumberOfTokens = 3;
   if (tokens.size() < minExpectedNumberOfTokens) {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Incorrect number of tokens for "  
		 << BeamToAtaBeamsKeyword << ": " 
		 << line << endl;
      return;
   }

   // extract component info from lines
   const string & beamName = tokens[1];

   // Make sure there's not already a beam-to-ata-beams association
   // in the list for this beam.
   if (beamToAtaBeamsMap_.find(beamName) != beamToAtaBeamsMap_.end())
   {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Repeated beam for " << BeamToAtaBeamsKeyword
		 << ": " << beamName << endl;
      return;
   }

   // TBD: verify that beam number is also in the component list

   // grab each atabeam and associate it with the beam
   for (unsigned int tokenIndex=2; tokenIndex < tokens.size(); ++tokenIndex)
   {
      const string & ataBeamName(tokens[tokenIndex]);

      // Make sure the ata beam is not already assigned
      for (BeamToAtaBeamsMultiMap::iterator it = beamToAtaBeamsMap_.begin();
	   it !=  beamToAtaBeamsMap_.end(); ++it)
      {
	 if (it->second == ataBeamName)
	 {
	    printErrorLine(errorStrm_, linenum, line);
	    errorStrm_ << "Repeated ata beam for " << BeamToAtaBeamsKeyword
		       << ": " << ataBeamName << endl;
	    return;
	 }
      }

      // store association
      beamToAtaBeamsMap_.insert(make_pair(beamName, ataBeamName));
   }

}

// Add a chan-to-beams association
// Tokens should be in this form:
// <chanToBeamsKeyword> <beam name> <channelizer name> [<channelizer name> ...]

void ExpectedNssComponentsTreeInternal::
addChanToBeamsAssociation(
   const string &line,      // original line, for error reporting
   int linenum,             // line number
   vector<string> &tokens)  // line broken into tokens
{
   Assert(tokens.size() > 0);
   Assert(tokens[0] == ChanToBeamsKeyword);

   // make sure there are the correct number of tokens
   size_t minExpectedNumberOfTokens = 3;
   if (tokens.size() < minExpectedNumberOfTokens) {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Incorrect number of tokens for "  
		 << ChanToBeamsKeyword << ": " 
		 << line << endl;
      return;
   }

   // extract component info from lines
   const string & beamName = tokens[1];

   // Make sure there's not already a chan-to-beams association
   // in the list for this beam.
   if (chanToBeamsMap_.find(beamName) != chanToBeamsMap_.end())
   {
      printErrorLine(errorStrm_, linenum, line);
      errorStrm_ << "Repeated beam for " << ChanToBeamsKeyword
		 << ": " << beamName << endl;
      return;
   }

   // TBD: verify that beam number is also in the component list

   // grab each atabeam and associate it with the beam
   for (unsigned int tokenIndex=2; tokenIndex < tokens.size(); ++tokenIndex)
   {
      const string & chanName(tokens[tokenIndex]);

      // Make sure the ata beam is not already assigned
      for (ChannelizerToBeamsMultiMap::iterator it = chanToBeamsMap_.begin();
	   it !=  chanToBeamsMap_.end(); ++it)
      {
	 if (it->second == chanName)
	 {
	    printErrorLine(errorStrm_, linenum, line);
	    errorStrm_ << "Repeated channelizer name for " << ChanToBeamsKeyword
		       << ": " << chanName << endl;
	    return;
	 }
      }

      // store association
      chanToBeamsMap_.insert(make_pair(beamName, chanName));
   }

}

string ExpectedNssComponentsTreeInternal::getParent(
   const string &componentName)
{
   ComponentInfo *info = findComponentInfoByName(componentName);
   if (info == 0)
   {
      return "";  
   }
   return info->parentName_;
}



// ------------ end ExpectedNssComponentsTreeInternal --------------

// ------------ begin ExpectedNssComponentsTree --------------


ExpectedNssComponentsTree::ExpectedNssComponentsTree(const string &configFilename,
						     ostream &errorStrm)
{
   // TBD file access error handling
   vector<string> configVector = SseUtil::loadFileIntoStringVector(configFilename);
   internal_ = new ExpectedNssComponentsTreeInternal(configVector, errorStrm);
}

ExpectedNssComponentsTree::ExpectedNssComponentsTree(vector<string> &configVector,
						     ostream &errorStrm)
   : internal_(new ExpectedNssComponentsTreeInternal(configVector, errorStrm))
{
}

ExpectedNssComponentsTree::ExpectedNssComponentsTree(ostream &errorStrm)
{
   // Make an empty (dummy) version

   vector<string> configVector;
   configVector.push_back(ConfigFileVersion);
   internal_ = new ExpectedNssComponentsTreeInternal(configVector, errorStrm);
}


ExpectedNssComponentsTree::~ExpectedNssComponentsTree()
{
   delete internal_;
}


// output all the component names
ostream& operator << (ostream &strm, const ExpectedNssComponentsTree &tree)
{

   strm << "Configuration file: " << endl;
   strm << "----------------------" << endl;
   tree.internal_->printRawConfigLines(strm);
   strm << endl;

   strm << "Parsed components: " << endl;
   strm << "----------------------" << endl;
   tree.internal_->printComponents(strm);
   strm << endl;

   strm << "BeamToAtaBeams associations: " << endl;
   strm << "-----------------------------" << endl;
   tree.internal_->printBeamToAtaBeamsAssociations(strm);
   strm << endl;

   return strm;
}


vector<string> ExpectedNssComponentsTree::getAllComponents()
{
   return internal_->getAllComponentNames();
}

vector<string> ExpectedNssComponentsTree::getSites()
{
   return internal_->findComponentNamesByType(SiteType);
}

vector<string> ExpectedNssComponentsTree::getIfcs()
{
   return internal_->findComponentNamesByType(IfcType);
}

vector<string> ExpectedNssComponentsTree::getBeams()
{
   return internal_->findComponentNamesByType(BeamType);
}

vector<string> ExpectedNssComponentsTree::getDxs()
{
   return internal_->findComponentNamesByType(DxType);
}

vector<string> ExpectedNssComponentsTree::getZxs()
{
   return internal_->findComponentNamesByType(ZxType);
}

vector<string> ExpectedNssComponentsTree::getChans()
{
   return internal_->findChannelizerNames();
}


vector<string> ExpectedNssComponentsTree::getDxsForIfc(const string & ifcName)
{
   // get the dxs on each beam associated with this ifc
   vector<string> beams = internal_->findComponentChildrenNames(IfcType, ifcName);
   vector<string> totalDxs;
   vector<string>::iterator beamIt;
   for (beamIt = beams.begin(); beamIt != beams.end(); ++beamIt)
   {
      string & beamName = *beamIt;
      vector<string> dxs = internal_->findComponentChildrenNames(BeamType,
								  beamName, DxType);
      totalDxs.insert(totalDxs.end(), dxs.begin(), dxs.end());
   }

   return totalDxs;
}

vector<string> ExpectedNssComponentsTree::getZxsForIfc(const string & ifcName)
{
   // get the zxs on each beam associated with this ifc
   vector<string> beams = internal_->findComponentChildrenNames(IfcType, ifcName);

   vector<string> totalZxs;
   vector<string>::iterator beamIt;
   for (beamIt = beams.begin(); beamIt != beams.end(); ++beamIt)
   {
      string & beamName = *beamIt;
      vector<string> zxs = internal_->findComponentChildrenNames(BeamType,
								  beamName, ZxType);
      totalZxs.insert(totalZxs.end(), zxs.begin(), zxs.end());
   }

   return totalZxs;
}

vector<string> ExpectedNssComponentsTree::getDxsForBeam(const string & beamName)
{
   return internal_->findComponentChildrenNames(BeamType, beamName, DxType);
}

vector<string> ExpectedNssComponentsTree::getZxsForBeam(const string & beamName)
{
   return internal_->findComponentChildrenNames(BeamType, beamName, ZxType);
}

vector<string> ExpectedNssComponentsTree::getChansForBeam(const string & beamName)
{
   return internal_->findChannelizerNames(beamName);
}

vector<string> ExpectedNssComponentsTree::getBeamsForIfc(const string & ifcName)
{
   return internal_->findComponentChildrenNames(IfcType, ifcName);
}

vector<string> ExpectedNssComponentsTree::getIfcsForSite(const string & siteName)
{
   return internal_->findComponentNamesByTypeAndSite(IfcType, siteName);
}

vector<string> ExpectedNssComponentsTree::getDxsForSite(const string & siteName)
{
   return internal_->findComponentNamesByTypeAndSite(DxType, siteName);
}

vector<string> ExpectedNssComponentsTree::getZxsForSite(const string & siteName)
{
   return internal_->findComponentNamesByTypeAndSite(ZxType, siteName);
}

vector<string> ExpectedNssComponentsTree::getChansForSite(const string & siteName)
{
   return internal_->findChannelizerNames();
}

string ExpectedNssComponentsTree::getBeamForDx(const string &dx)
{
   return internal_->getParent(dx);
}

string ExpectedNssComponentsTree::getBeamForZx(const string &zx)
{
   return internal_->getParent(zx);
}

string ExpectedNssComponentsTree::getBeamForChan(const string &chan)
{
      for (ChannelizerToBeamsMultiMap::iterator it = internal_->chanToBeamsMap_.begin();
	   it !=  internal_->chanToBeamsMap_.end(); ++it)
      {
	 if (it->second == chan)
	 {
	    return it->first;
	 }
      }
   return("");
}

string ExpectedNssComponentsTree::getIfcForDx(const string &dx)
{
   return internal_->getParent(getBeamForDx(dx));
}

string ExpectedNssComponentsTree::getIfcForZx(const string &zx)
{
   return internal_->getParent(getBeamForZx(zx));
}

vector<string> ExpectedNssComponentsTree::getAtaBeamsForBeam(const string &beam)
{
   vector<string> ataBeams;

   BeamToAtaBeamsMultiMap::iterator it;
   for (it = internal_->beamToAtaBeamsMap_.lower_bound(beam);
	it != internal_->beamToAtaBeamsMap_.upper_bound(beam); ++it)
   {
      ataBeams.push_back(it->second);
   }

   return ataBeams;
}

vector<string> ExpectedNssComponentsTree::getAtaBeams()
{
   vector<string> ataBeams;

   for (BeamToAtaBeamsMultiMap::iterator it = 
	   internal_->beamToAtaBeamsMap_.begin();
	it != internal_->beamToAtaBeamsMap_.end(); it++)
   {
      ataBeams.push_back(it->second);
   }

   return ataBeams;
}
