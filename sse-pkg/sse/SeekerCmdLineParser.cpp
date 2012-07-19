/*******************************************************************************

 File:    SeekerCmdLineParser.cpp
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


#include "SeekerCmdLineParser.h" 
#include "CmdLineParser.h"
#include "ssePorts.h"
#include "SseUtil.h"

#define DEFAULT_DX_ARCHIVER1_HOSTNAME "localhost"
#define DEFAULT_DX_ARCHIVER2_HOSTNAME "localhost"
#define DEFAULT_DX_ARCHIVER3_HOSTNAME "localhost"
#define DEFAULT_EXPECTED_COMPONENTS_FILENAME "expectedSonATAComponents.cfg"

SeekerCmdLineParser::SeekerCmdLineParser()
   :
   parser(0),
   dxPortArg("--dx-port"),
   zxPortArg("--zx-port"),
   ifcPortArg("--ifc-port"),
   tsigPortArg("--tsig-port"),
   tscopePortArg("--tscope-port"),
   channelizerPortArg("--channelizer-port"),
   dxArchiverPortArg("--dx-archiver-port"),
   dxArchiver1HostnameArg("--dx-archiver1-hostname"),
   dxToArchiver1PortArg("--dx-to-dx-archiver1-port"),
   dxArchiver2HostnameArg("--dx-archiver2-hostname"),
   dxToArchiver2PortArg("--dx-to-dx-archiver2-port"),
   dxArchiver3HostnameArg("--dx-archiver3-hostname"),
   dxToArchiver3PortArg("--dx-to-dx-archiver3-port"),
   expectedComponentsFilenameArg("--expected-components-file"),
   componentControlPortArg("--component-control-port"),
   noUiArg("--noui")
{
   parser = createCmdLineParser();
}

SeekerCmdLineParser::~SeekerCmdLineParser()
{
   delete parser;
}

CmdLineParser * SeekerCmdLineParser::createCmdLineParser()
{
   CmdLineParser * parser = new CmdLineParser();
      
   parser->addIntOption(dxPortArg, SseUtil::strToInt(DEFAULT_DX_PORT),
                        "port for dx connections");

   parser->addIntOption(zxPortArg, SseUtil::strToInt(DEFAULT_ZX_PORT),
                        "port for zx connections");

   parser->addIntOption(ifcPortArg, SseUtil::strToInt(DEFAULT_IFC_PORT),
                        "port for ifc connections");

   parser->addIntOption(tsigPortArg, SseUtil::strToInt(DEFAULT_TSIG_PORT),
                        "port for tsig connections");

   parser->addIntOption(tscopePortArg, SseUtil::strToInt(DEFAULT_TSCOPE_PORT),
                        "port for tscope connections");

   parser->addIntOption(channelizerPortArg, 
                        SseUtil::strToInt(DEFAULT_CHANNELIZER_PORT),
                        "port for channelizer connections");

   parser->addIntOption(dxArchiverPortArg,
                        SseUtil::strToInt(DEFAULT_DX_ARCHIVER_TO_SSE_PORT),
                        "port for dx archiver connections");

   parser->addStringOption(dxArchiver1HostnameArg,
                           DEFAULT_DX_ARCHIVER1_HOSTNAME,
                           "dx archiver1 hostname");

   parser->addIntOption(dxToArchiver1PortArg,
                        SseUtil::strToInt(DEFAULT_DX_TO_DX_ARCHIVER1_PORT),
                        "port for dx connections to archiver1");

   parser->addStringOption(dxArchiver2HostnameArg, DEFAULT_DX_ARCHIVER2_HOSTNAME,
                           "dx archiver2 hostname");

   parser->addIntOption(dxToArchiver2PortArg,
                        SseUtil::strToInt(DEFAULT_DX_TO_DX_ARCHIVER2_PORT),
                        "port for dx connections to archiver2");

   parser->addStringOption(dxArchiver3HostnameArg, DEFAULT_DX_ARCHIVER3_HOSTNAME,
                           "dx archiver3 hostname");

   parser->addIntOption(dxToArchiver3PortArg,
                        SseUtil::strToInt(DEFAULT_DX_TO_DX_ARCHIVER3_PORT),
                        "port for dx connections to archiver3");

   parser->addStringOption(expectedComponentsFilenameArg,
                           DEFAULT_EXPECTED_COMPONENTS_FILENAME,
                           "expected NSS components configuration filename");

   parser->addIntOption(componentControlPortArg,
                        SseUtil::strToInt(DEFAULT_COMPONENT_CONTROL_PORT),
                        "port for component control connections");

   parser->addFlagOption(noUiArg,"disable user interface (server mode)");

   return parser;
}

bool SeekerCmdLineParser::parse(int argc, char * argv[])
{
   return parser->parse(argc, argv);
}
   
string SeekerCmdLineParser::getErrorText()
{
   return parser->getErrorText();
}

string SeekerCmdLineParser::getUsage()
{
   return parser->getUsage();
}

string SeekerCmdLineParser::getDxPort()
{
   return parser->getStringOption(dxPortArg);
}

string SeekerCmdLineParser::getZxPort()
{
   return parser->getStringOption(zxPortArg);
}

string SeekerCmdLineParser::getIfcPort()
{
   return parser->getStringOption(ifcPortArg);
}

string SeekerCmdLineParser::getTsigPort()
{
   return parser->getStringOption(tsigPortArg);
}

string SeekerCmdLineParser::getTscopePort()
{
   return parser->getStringOption(tscopePortArg);
}

string SeekerCmdLineParser::getChannelizerPort()
{
   return parser->getStringOption(channelizerPortArg);
}

string SeekerCmdLineParser::getDxArchiverPort()
{
   return parser->getStringOption(dxArchiverPortArg);
}

string SeekerCmdLineParser::getDxArchiver1Hostname()
{
   return parser->getStringOption(dxArchiver1HostnameArg);
}

string SeekerCmdLineParser::getDxToArchiver1Port()
{
   return parser->getStringOption(dxToArchiver1PortArg);
}

string SeekerCmdLineParser::getDxArchiver2Hostname()
{
   return parser->getStringOption(dxArchiver2HostnameArg);
}

string SeekerCmdLineParser::getDxToArchiver2Port()
{
   return parser->getStringOption(dxToArchiver2PortArg);
}

string SeekerCmdLineParser::getDxArchiver3Hostname()
{
   return parser->getStringOption(dxArchiver3HostnameArg);
}

string SeekerCmdLineParser::getDxToArchiver3Port()
{
   return parser->getStringOption(dxToArchiver3PortArg);
}

string SeekerCmdLineParser::getComponentControlPort()
{
   return parser->getStringOption(componentControlPortArg);
}

string SeekerCmdLineParser::getExpectedComponentsFilename()
{
   return parser->getStringOption(expectedComponentsFilenameArg);
}

bool SeekerCmdLineParser::getNoUi()
{
   return parser->getFlagOption(noUiArg);
}
