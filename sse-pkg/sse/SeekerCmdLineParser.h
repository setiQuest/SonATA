/*******************************************************************************

 File:    SeekerCmdLineParser.h
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


#ifndef SeekerCmdLineParser_H
#define SeekerCmdLineParser_H

#include <string>
using std::string;

class CmdLineParser;

class SeekerCmdLineParser
{
 public:
   SeekerCmdLineParser();
   virtual ~SeekerCmdLineParser();
    
   bool parse(int argc, char * argv[]);
   string getErrorText();
   string getUsage();
   string getDxPort();
   string getZxPort();
   string getIfcPort();
   string getTsigPort();
   string getTscopePort();
   string getChannelizerPort();
   string getDxArchiverPort();
   string getDxArchiver1Hostname();
   string getDxToArchiver1Port();
   string getDxArchiver2Hostname();
   string getDxToArchiver2Port();
   string getDxArchiver3Hostname();
   string getDxToArchiver3Port();
   string getComponentControlPort();
   string getExpectedComponentsFilename();
   bool getNoUi();

 private:

   CmdLineParser *parser;
   string dxPortArg;
   string zxPortArg;
   string ifcPortArg;
   string tsigPortArg;
   string tscopePortArg;
   string channelizerPortArg;
   string dxArchiverPortArg;
   string dxArchiver1HostnameArg;
   string dxToArchiver1PortArg;
   string dxArchiver2HostnameArg;
   string dxToArchiver2PortArg;
   string dxArchiver3HostnameArg;
   string dxToArchiver3PortArg;
   string expectedComponentsFilenameArg;
   string componentControlPortArg;
   string noUiArg;

   CmdLineParser * createCmdLineParser();

   // Disable copy construction & assignment.
   // Don't define these.
   SeekerCmdLineParser(const SeekerCmdLineParser& rhs);
   SeekerCmdLineParser& operator=(const SeekerCmdLineParser& rhs);

};

#endif // SeekerCmdLineParser_H
