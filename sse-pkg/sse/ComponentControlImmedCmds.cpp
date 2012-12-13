/*******************************************************************************

 File:    ComponentControlImmedCmds.cpp
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



#include "ComponentControlImmedCmds.h" 
#include "ComponentControlList.h"
#include "ComponentControlProxy.h"
#include "Site.h"

static const string allComponents("all");

ComponentControlImmedCmds::ComponentControlImmedCmds(const string &cmdName) :
   cmdName_(cmdName),
   site_(0)
{
}

ComponentControlImmedCmds::~ComponentControlImmedCmds()
{
}

void ComponentControlImmedCmds::setSite(Site *site)
{
   site_ = site;
}

Site * ComponentControlImmedCmds::getSite() const
{
   return site_;
}

const string & ComponentControlImmedCmds::getCommand() const
{
   return cmdName_;
}


static void getAllComponentControls(ComponentControlList &allComponentControls, Site *site)
{
   Assert(site);
   site->componentControlManager()->getProxyList(&allComponentControls);
}


bool ComponentControlImmedCmds::controllersAvailable()
{
   ComponentControlList allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   return (allComponentControls.size() > 0);
}


bool ComponentControlImmedCmds::isComponentUnderControl(const string &componentName)
{
   ComponentControlList allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);
   for (ComponentControlList::const_iterator componentControl = allComponentControls.begin();
	componentControl != allComponentControls.end(); componentControl++)
   {
      ComponentControlStatus status = (*componentControl)->getStatus();
      
      // see if the component is listed in the status
      if (status.text.find(componentName) != string::npos)
      {
	 found  = true;
	 break;
      }
   }

   return found;
}

const char * ComponentControlImmedCmds::printCmdFeedback(stringstream & strm,
					    bool success,
					    const string &cmdName,
					    const string &methodName, 
					    const string &componentControlName) const
{
   strm << cmdName << "::" << methodName; 

   if (success)
   {
      strm << " command sent to componentControls: ";
   }
   else
   {
      strm << ": the requested componentControls(s) are not connected: ";
   }
   strm << componentControlName << endl;

   outString_ = strm.str();
   return(outString_.c_str());

}


const char * ComponentControlImmedCmds::names() const
{
   ComponentControlList allComponentControls;
   getAllComponentControls(allComponentControls, getSite());
   
   stringstream strm;
   for (ComponentControlList::const_iterator componentControl = allComponentControls.begin();
	componentControl != allComponentControls.end(); componentControl++)
   {
      strm << (*componentControl)->getName() << " ";
   }
   strm << endl;

   outString_ = strm.str();
   return(outString_.c_str());
}

const char* ComponentControlImmedCmds::intrin(const string& componentControlName) const
{
   const string methodName("intrin");

   ComponentControlList  allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);
   stringstream strm;
   for (ComponentControlList::iterator componentControl = allComponentControls.begin(); 
	componentControl != allComponentControls.end(); componentControl++)
   {
      if (componentControlName == (*componentControl)->getName()
	  || componentControlName == allComponents)
      {
	 strm << (*componentControl)->getIntrinsics() << endl;
	 found  = true;
      } 
   }

   return printCmdFeedback(strm, found, getCommand(), methodName, componentControlName);
}



const char * ComponentControlImmedCmds::reset(const string& componentControlName)
{
   const string methodName("reset");

   ComponentControlList allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);
   for (ComponentControlList::iterator componentControl = allComponentControls.begin();
	componentControl != allComponentControls.end(); componentControl++)
   {
      if (componentControlName == (*componentControl)->getName() ||
	  componentControlName == allComponents)
      {
	 (*componentControl)->reset();

	 found = true;
      } 
   }

   stringstream strm;
   return printCmdFeedback(strm, found, getCommand(), methodName, componentControlName);
}

const char * ComponentControlImmedCmds::reqstat(const string& componentControlName) const
{
   const string methodName("reqstat");

   ComponentControlList allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);

   for (ComponentControlList::const_iterator componentControl = allComponentControls.begin(); 
	componentControl != allComponentControls.end(); componentControl++)
   {
      if (componentControlName == (*componentControl)->getName() 
	  || componentControlName == allComponents)
      {
	 (*componentControl)->requestStatusUpdate();
	 found  = true;
      } 
   }

   stringstream strm;
   return printCmdFeedback(strm, found, getCommand(), methodName, componentControlName);
}

const char * ComponentControlImmedCmds::send(const string& componentControlName, const string& command,
			const string & cmdarg1, const string & cmdarg2,
			const string & cmdarg3, const string & cmdarg4,  
			const string & cmdarg5)
{
   const string methodName("send");

   ComponentControlList  allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);
   for (ComponentControlList::iterator componentControl = allComponentControls.begin(); componentControl != allComponentControls.end(); componentControl++)
   {
      if (componentControlName == (*componentControl)->getName() 
	  || componentControlName == allComponents)
      {
	 string cmdWithArgs = command + " " + cmdarg1 + " " + cmdarg2 
	    + " " + cmdarg3 + " " + cmdarg4 + " " + cmdarg5; 

	 (*componentControl)->sendCommand(cmdWithArgs);
	 found = true;
      }
   }

   stringstream strm;
   return printCmdFeedback(strm, found, getCommand(), methodName, componentControlName);
}


const char * ComponentControlImmedCmds::start(
   const string& componentName0,
   const string& componentName1,
   const string& componentName2,
   const string& componentName3,
   const string& componentName4,
   const string& componentName5,
   const string& componentName6,
   const string& componentName7,
   const string& componentName8,
   const string& componentName9)
{
   string command("start");

   return sendCommandToAllControllers(command, componentName0,
				      componentName1, componentName2,
				      componentName3, componentName4,
				      componentName5, componentName6,
				      componentName7, componentName8,
				      componentName9);
}

/* 
   send a kill on to components controller
   for all listed components
 */

const char * ComponentControlImmedCmds::shutdown(
   const string& componentName0,
   const string& componentName1,
   const string& componentName2,
   const string& componentName3,
   const string& componentName4,
   const string& componentName5,
   const string& componentName6,
   const string& componentName7,
   const string& componentName8,
   const string& componentName9)
{
   string command("kill");

   return sendCommandToAllControllers(command, componentName0,
				      componentName1, componentName2,
				      componentName3, componentName4,
				      componentName5, componentName6,
				      componentName7, componentName8,
				      componentName9);
}


const char * ComponentControlImmedCmds::restart(
   const string& componentName0,
   const string& componentName1,
   const string& componentName2,
   const string& componentName3,
   const string& componentName4,
   const string& componentName5,
   const string& componentName6,
   const string& componentName7,
   const string& componentName8,
   const string& componentName9)
{
   string command("restart");

   return sendCommandToAllControllers(command, componentName0,
				      componentName1, componentName2,
				      componentName3, componentName4,
				      componentName5, componentName6,
				      componentName7, componentName8,
				      componentName9);
}

// Tell the component controllers to start the listed components.

const char * ComponentControlImmedCmds::sendCommandToAllControllers(
   const string& command,
   const string& componentName0,
   const string& componentName1,
   const string& componentName2,
   const string& componentName3,
   const string& componentName4,
   const string& componentName5,
   const string& componentName6,
   const string& componentName7,
   const string& componentName8,
   const string& componentName9)
{
   const string methodName("sendCommandToAllControllers");

   ComponentControlList  allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);
   for (ComponentControlList::iterator componentControl = 
	   allComponentControls.begin();
	componentControl != allComponentControls.end(); 
	componentControl++)
   {
      string cmdWithArgs = command + " " + componentName0 + " "
	 + componentName1 + " " + componentName2 + " "
	 + componentName3 + " " + componentName4 + " "
	 + componentName5 + " " + componentName6 + " "
	 + componentName7 + " " + componentName8 + " "
	 + componentName9;
      
      (*componentControl)->sendCommand(cmdWithArgs);
      found = true;
   }

   stringstream strm;
   return printCmdFeedback(strm, found, getCommand(), methodName, "");
}


const char* ComponentControlImmedCmds::status(const string& componentControlName) const
{
   const string methodName("status");

   ComponentControlList allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);
   stringstream strm;

   for (ComponentControlList::const_iterator componentControl = allComponentControls.begin(); componentControl != allComponentControls.end(); componentControl++)
   {
      if (componentControlName == (*componentControl)->getName()
	  || componentControlName == allComponents)
      {
	 strm << (*componentControl)->getStatus();
	 found  = true;
      } 
   }
   return printCmdFeedback(strm, found, getCommand(), methodName, componentControlName);
}


const char * ComponentControlImmedCmds::selfshutdown(const string& componentControlName)
{
   const string methodName("selfshutdown");

   ComponentControlList allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);

   for (ComponentControlList::iterator componentControl = allComponentControls.begin(); componentControl != allComponentControls.end(); componentControl++) {
      if (componentControlName == (*componentControl)->getName() || componentControlName == allComponents)
      {
	 (*componentControl)->shutdown();
	 found = true;
      } 
   }

   stringstream strm;
   return printCmdFeedback(strm, found, getCommand(), methodName, componentControlName);
}

const char * ComponentControlImmedCmds::resetsocket(const string& componentControlName)
{
   const string methodName("resetsocket");

   ComponentControlList  allComponentControls;
   getAllComponentControls(allComponentControls, getSite());

   bool found(false);

   for (ComponentControlList::iterator componentControl = allComponentControls.begin(); componentControl != allComponentControls.end(); componentControl++)
   {
      if (componentControlName == (*componentControl)->getName() || componentControlName == allComponents)
      {
	 (*componentControl)->resetSocket();
	 found = true;
      } 
   }

   stringstream strm;
   return printCmdFeedback(strm, found, getCommand(), methodName, componentControlName);
}



// indent to the requested level
static void indent(ostream &os, int level)
{
   for (int i = 0; i < level; i++)
   {
      os << "  ";
   } 
}


static void printImmedCmdHelp(ostream &os, const string &helpText) 
{
   int indentLevel = 3;
   indent(os, indentLevel);
   os << helpText << endl;
}

void ComponentControlImmedCmds::help(ostream& os) const
{
   int indentLevel = 1;
   indent(os, indentLevel);
   os << "Command: \"" << getCommand() << "\"" << endl;

   indentLevel = 2;
   indent(os, indentLevel);
   os << "Immediate Commands for controlling components:" << endl;

   printImmedCmdHelp(os, "start <component1> [<componentN> ...] \t\tstart components");
   printImmedCmdHelp(os, "shutdown <'all' | component1> [<componentN> ...] \tshutdown components");
   printImmedCmdHelp(os, "restart <'all' | component1> [<componentN> ...] \trestart components (shutdown/start)");
   os << endl;

   indent(os, indentLevel);
   os << "Immediate Commands for component controller itself:" << endl;

   printImmedCmdHelp(os, "help \t\t\t\t\tprint this help text");
   printImmedCmdHelp(os, "intrin [<name>='all'] \t\t\tdisplay ComponentControl intrinsics");
   printImmedCmdHelp(os, "names \t\t\t\t\tlist names of all connected ComponentControls");
   printImmedCmdHelp(os, "reqstat [<name>='all'] \t\t\trequest ComponentControl status update");
   printImmedCmdHelp(os, "reset <name | 'all'> \t\t\treset ComponentControl(s)");
   printImmedCmdHelp(os, "resetsocket <name | 'all'> \t\treset ComponentControl socket");
   printImmedCmdHelp(os, "send <name | 'all'> <command> [arg1] [arg2] ... [arg5] \n\t\t\t\t\t\tsend command with arguments to ComponentControl(s)");
   printImmedCmdHelp(os, "selfshutdown <name | 'all'> \t\tshutdown ComponentControl(s)");
   printImmedCmdHelp(os, "status [<name>='all'] \t\t\tdisplay status of ComponentControl(s)");

}

