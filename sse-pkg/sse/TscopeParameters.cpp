/*******************************************************************************

 File:    TscopeParameters.cpp
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


#include <ace/OS.h>            // for SwigScheduler.h
#include "TscopeParameters.h"
#include "TscopeProxy.h"
#include "Site.h"
#include "Assert.h"
#include "SseMsg.h"
#include "SseTscopeMsg.h"
#include "RangeParameter.h"
#include "ChoiceParameter.h"
#include "AnyValueParameter.h"
#include "AtaInformation.h"
#include "ComponentControlImmedCmds.h"
#include "SseCommUtil.h"
#include <iostream>
#include <memory>  // for auto_ptr

using namespace std;

extern ComponentControlImmedCmds componentControlImmedCmdsGlobal;

static const char *DbTableName="TscopeParameters";
static const char *IdColNameInActsTable="TscopeParametersId";

const double DefaultRfSkyFreqMhz =  AtaInformation::AtaDefaultSkyFreqMhz;

// For Prelude Beamformer tune offset when running in '--sideband upper' mode
//  was -19 and 6.
// Beamformer tune offset when running SonATA is 0
const double DefaultBasebandTuneOffsetMhz = 0;
const double DefaultBasebandCenterTuneOffsetMhz = 0;

static const char *AntListSourceParam = "param";
static const char *AntListSourceAntGroup = "antgroup";
static const char *AntListDefault = "9z";  
static const int AntsMaxSefdDefaultJy = 20000;
static const double AtaDefaultHorizDeg = 18;

struct TscopeParametersInternal
{
   RangeParameter<double> tuningASkyFreqMhz;
   RangeParameter<double> tuningBSkyFreqMhz;
   RangeParameter<double> tuningCSkyFreqMhz;
   RangeParameter<double> tuningDSkyFreqMhz;

   RangeParameter<double> basebandTuneOffsetMhz;
   RangeParameter<double> basebandCenterTuneOffsetMhz;

   RangeParameter<double> primaryFovAtOneGhzDeg;
   RangeParameter<double> beamsizeAtOneGhzArcSec;

   ChoiceParameter<string> calType;
   RangeParameter<int> calTimeSecs;
   RangeParameter<int> calNumCycles;
   RangeParameter<int> calIterations;

   RangeParameter<double> siteLatNorthDeg;
   RangeParameter<double> siteLongWestDeg;
   RangeParameter<double> siteHorizDeg;
   AnyValueParameter<string> siteName;

   ChoiceParameter<string> antlistSource;
   AnyValueParameter<string> xpolAnts;
   AnyValueParameter<string> ypolAnts;
   AnyValueParameter<string> primaryAnts;

   // -- methods & helper vars ---
   TscopeParametersInternal();
   void setCommand(const string & command);
   bool validPointRequestRaHours(ostream &errorStrm, double raHours);
   bool validPointRequestRaDeg(ostream &errorStrm, double raDeg);
   bool validPointRequestDecDeg(ostream &errorStrm, double decDeg);

   mutable string outString; // string buffer for tcl
   string command;
};

TscopeParametersInternal::TscopeParametersInternal():

   tuningASkyFreqMhz("tuninga", "MHz", "sky freq for ATA tuning 'A'", 
                     DefaultRfSkyFreqMhz, AtaInformation::AtaMinSkyFreqMhz,
                     AtaInformation::AtaMaxSkyFreqMhz),

   tuningBSkyFreqMhz("tuningb", "MHz", "sky freq for ATA tuning 'B'", 
                     DefaultRfSkyFreqMhz, AtaInformation::AtaMinSkyFreqMhz,
                     AtaInformation::AtaMaxSkyFreqMhz),

   tuningCSkyFreqMhz("tuningc", "MHz", "sky freq for ATA tuning 'C'", 
                     DefaultRfSkyFreqMhz, AtaInformation::AtaMinSkyFreqMhz,
                     AtaInformation::AtaMaxSkyFreqMhz),

   tuningDSkyFreqMhz("tuningd", "MHz", "sky freq for ATA tuning 'D'", 
                     DefaultRfSkyFreqMhz, AtaInformation::AtaMinSkyFreqMhz, 
                     AtaInformation::AtaMaxSkyFreqMhz),

   basebandTuneOffsetMhz("tuneoffset", "MHz", 
                         "baseband tune offset (automatically added to \n\t\t\t\ttuning[a-d])",
                         DefaultBasebandTuneOffsetMhz, -50.0, 50.0),

   basebandCenterTuneOffsetMhz(
      "centertuneoffset", "MHz",
      "center tune offset (diff between SonATA band \n\t\t\t\tcenter and bf band center)",
      DefaultBasebandCenterTuneOffsetMhz, -50.0, 50.0),

   primaryFovAtOneGhzDeg("primaryfov", "deg @1GHz", "primary Field-Of-View at 1 GHz", 
                         AtaInformation::AtaPrimaryFovAt1GhzDeg, 0, 5),

   beamsizeAtOneGhzArcSec("beamsize", "arcsec @1GHz", "synth beamsize at 1 GHz",
                          AtaInformation::Ata42BeamsizeAt1GhzArcSec, 
                          0, AtaInformation::Ata1BeamsizeAt1GhzArcSec),

   calType("caltype",  "", "calibration type",
      SseTscopeMsg::calTypeToString(TscopeCalRequest::PHASE)),

   calTimeSecs("caltime", "secs", "calibration integration time",
      120, 1, 1200),

   calNumCycles("calcycles", "count", "number of times calibration is repeated",
                2, 1, 10),

   //Iterate calibrations - JR
   calIterations("caliterations", "", "number of bf calibration iterations",
               0, 0, 5),

   siteLatNorthDeg("sitelat", "north deg", "site latitude", 
                   AtaInformation::AtaLatNorthDeg, -90, 90),

   siteLongWestDeg("sitelong", "west deg", "site longitude", 
                   AtaInformation::AtaLongWestDeg, -180, 180),

   siteHorizDeg("sitehoriz", "deg", "site horizon", 
                AtaDefaultHorizDeg, 
                AtaInformation::AtaMinHorizonDeg, 90),

   siteName("sitename", "", "site name", "ATA"),

   antlistSource(
      "antlistsource", "", "where to specify the ant lists\n\t\t\t\t", 
      AntListSourceParam),

   xpolAnts(
      "antsxpol", "", "comma separated X pol ant names", 
      AntListDefault),

   ypolAnts(
      "antsypol", "", "comma separated Y pol ant names", 
      AntListDefault),

   primaryAnts(
      "antsprimary", "", "comma separated primary ant names", 
      AntListDefault)

{}

TscopeParameters::TscopeParameters(string command) : 
   SeekerParameterGroup(command, DbTableName, IdColNameInActsTable),
   internal_(new TscopeParametersInternal())
{
   internal_->calType.addChoice(
      SseTscopeMsg::calTypeToString(TscopeCalRequest::DELAY));

   internal_->calType.addChoice(
      SseTscopeMsg::calTypeToString(TscopeCalRequest::PHASE));

   internal_->calType.addChoice(
      SseTscopeMsg::calTypeToString(TscopeCalRequest::FREQ));

   internal_->antlistSource.addChoice(AntListSourceParam);
   internal_->antlistSource.addChoice(AntListSourceAntGroup);

   addParameters();
   addAllImmedCmdHelp();
   internal_->setCommand(command);
}

TscopeParameters::TscopeParameters(const TscopeParameters& rhs):
   SeekerParameterGroup(rhs.getCommand(), rhs.getDbTableName(), 
			rhs.getIdColNameInActsTable()),
   internal_(new TscopeParametersInternal(*rhs.internal_))
{
   setSite(rhs.getSite());
   addParameters();
   internal_->setCommand(rhs.getCommand());
}


TscopeParameters& TscopeParameters::operator=(const TscopeParameters& rhs)
{
   if (this == &rhs)
   {
      return *this;
   }
  
   setCommand(rhs.getCommand());
   eraseParamList();
   setSite(rhs.getSite());
   delete internal_;
   internal_ = new TscopeParametersInternal(*rhs.internal_);
   addParameters();
   return *this;
}

TscopeParameters::~TscopeParameters()
{
   delete internal_;
}


void TscopeParameters::addParameters()
{
   addParam(internal_->tuningASkyFreqMhz);
   addParam(internal_->tuningBSkyFreqMhz);
   addParam(internal_->tuningCSkyFreqMhz);
   addParam(internal_->tuningDSkyFreqMhz);
   addParam(internal_->basebandTuneOffsetMhz);
   addParam(internal_->basebandCenterTuneOffsetMhz);
   addParam(internal_->primaryFovAtOneGhzDeg);
   addParam(internal_->beamsizeAtOneGhzArcSec);
   addParam(internal_->calType);
   addParam(internal_->calTimeSecs);
   addParam(internal_->calNumCycles);
   addParam(internal_->calIterations); //JR
   addParam(internal_->siteLatNorthDeg);
   addParam(internal_->siteLongWestDeg);
   addParam(internal_->siteHorizDeg);
   addParam(internal_->siteName);
   addParam(internal_->antlistSource);
   addParam(internal_->xpolAnts);
   addParam(internal_->ypolAnts);
   addParam(internal_->primaryAnts);

   sort();
}


void TscopeParameters::addAllImmedCmdHelp() 
{
   addImmedCmdHelp("allocate <'antgroup' | antxx[,antxx...]> [<name='all']\n\t\t\t\tallocate subarray");
   addImmedCmdHelp("assign beamxxx <'antgroup' | antxx[,antxx...]> [<name='all'] \n\t\t\t\tdefine the antennas that make up a beam");
   addImmedCmdHelp("autoselectants <bf list like 1,2,3>  \n\t\t\t\tautomatically select ants for the antgroups\n");
   addImmedCmdHelp("bfcal <delay|phase|freq> integrate <secs> cycles <count> [<name='all']");
   addImmedCmdHelp("bfcal <delay|phase|freq> integrate <secs> cycles <count> [<name='all'] iterations <num>\n");
   addImmedCmdHelp("bfinit \t\t\tinit beamformer with all ants previously \n\t\t\t\tassigned to beams");
   addImmedCmdHelp("bfpoint \t\t\tpoint all beams previously assigned coordinates");
   addImmedCmdHelp("bfclearcoords \t\tclear all assigned beam coordinates");
   addImmedCmdHelp("bfclearants \t\tclear antenna assignments for all beams\n");
   
   addImmedCmdHelp("bfsetcoords beamName azel <Az deg> <El deg> [<name='all']");
   addImmedCmdHelp("bfsetcoords beamName gal <Long deg> <Lat deg> [<name='all']");
   addImmedCmdHelp("bfsetcoords beamName j2000 <RA hours> <Dec deg> [<name='all']");
   addImmedCmdHelp("bfsetcoords beamName j2000deg <RA deg> <Dec deg> [<name='all']\n");
   addImmedCmdHelp("bfreset \t\t\treset beamformer (requires recalibration)");
   addImmedCmdHelp("bfstop \t\t\tstop currently running beamformer command");

   addImmedCmdHelp("cleanup [<name='all'] \tdisconnect from telescope");
   addImmedCmdHelp("connect [<name='all'] \tconnect to telescope\n");
   addImmedCmdHelp("deallocate <'antgroup' | antxx[,antxx...]> [<name='all']\n\t\t\t\tdeallocate subarray\n");
   addImmedCmdHelp("disconnect [<name='all'] \tdisconnect from telescope");
   addImmedCmdHelp("intrin [<name>='all'] \tdisplay tscope intrinsics");
   addImmedCmdHelp("monitor <periodSecs> [<name='all'] monitor period");
   addImmedCmdHelp("names \t\t\tlist names of all connected tscopes\n");

   addImmedCmdHelp("point <'antgroup' | antxx[,antxx...]> azel <Az deg> <El deg> [<name='all']");
   addImmedCmdHelp("point <'antgroup' | antxx[,antxx...]> gal <Long deg> <Lat deg> [<name='all']");
   addImmedCmdHelp("point <'antgroup' | antxx[,antxx...]> j2000 <RA hours> <Dec deg> [<name='all']");
   addImmedCmdHelp("point <'antgroup' | antxx[,antxx...]> j2000deg <RA deg> <Dec deg> [<name='all']\n");

   addImmedCmdHelp("reqstat [<name>='all'] \trequest tscope status update");

//  addImmedCmdHelp("reset [<name='all'] \t\t reset tscopes(s)");
// reset currently does nothing

   addImmedCmdHelp("send <name | 'all'> <command> [arg1] ... [arg8] \n\t\t\t\tsend backend command with arguments");

   addImmedCmdHelp("resetsocket [<name='all']\treset tscope socket");
   addImmedCmdHelp("restart [<name='all']\trestart tscope(s)");
   addImmedCmdHelp("tune <tuning{a-d}> <sky freq MHz> [<name='all'] \n\t\t\t\tchange tuning sky frequency");
   addImmedCmdHelp("setup [<name='all'] \tconnect to telescope");
   addImmedCmdHelp("shutdown [<name='all'] \trun 'cleanup' & shutdown tscope server(s)");
   addImmedCmdHelp("sim [<name='all'] \tturn on simulator mode");
   addImmedCmdHelp("status [<name>='all'] \tdisplay status of tscope(s)\n");
   addImmedCmdHelp("stop <'antgroup' | antxx[,antxx...]> <name |'all'> \n\t\t\t\tstop antenna array");
   addImmedCmdHelp("stow <'antgroup' | antxx[,antxx...]> <name |'all'> \n\t\t\t\tstow antenna array");
   addImmedCmdHelp("unsim [<name='all'] \tturn off simulator mode");
   addImmedCmdHelp("wrap <'antgroup' | antxx[,antxx...]> <wrap number = 0, 1, 2> [<name='all']   \n\t\t\t\tset array wrap");
   addImmedCmdHelp("zfocus <'antgroup' | antxx[,antxx...]> <sky freq MHz> [<name='all'] \n\t\t\t\tchange zfocus sky frequency");
}

// A base class that runs through all the tscopes in a list,
// executing the operation (which is deferred to the subclass).

class TscopeOperation
{
public:

   string runThroughTscopeList(const string &tscopeName,
                               Site *site);
   virtual ~TscopeOperation();
protected:
   // subclasses override this method
   virtual string callOperation(TscopeProxy *proxy) = 0; 

};

TscopeOperation::~TscopeOperation()
{
}

string TscopeOperation::runThroughTscopeList(const string &tscopeName,
                                             Site *site)
{
   Assert(site);
   TscopeList tscopeList;
   site->tscopeManager()->getProxyList(&tscopeList);

   string resultString;

   bool found = false;
   for (TscopeList::iterator index = tscopeList.begin();
	index != tscopeList.end(); ++index)
   {
      TscopeProxy *tscopeProxy = *index;
      Assert(tscopeProxy);
      if ( (tscopeName == "all") || tscopeProxy->getName() == tscopeName)
      {
	 found = true;
	 resultString += callOperation(tscopeProxy);
      }
   }

   if (found)
   {
      // if the command had no output text of its own,
      // return a generic acknowledgement.
      if (resultString.empty())
      {
	 resultString =  "tscope command sent.";
      }
   }
   else 
   {
      resultString = "The requested tscope(s) are not connected: " + tscopeName;
   }

   return resultString;

}

const char * TscopeParameters::runSubarrayCmd(
   const string &subarray,
   TscopeOperation &operation,
   const string & tscopeName)
{
   stringstream strm;
   string errorText;
   if (!SseCommUtil::validAtaSubarray(subarray, errorText))
   {
      strm << errorText;
      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }
   
   return runCmd(operation, tscopeName);
}

const char * TscopeParameters::runCmd(
   TscopeOperation &operation,
   const string & tscopeName)
{
   internal_->outString = operation.runThroughTscopeList(tscopeName, getSite());
   return internal_->outString.c_str();
}

//----------------------------------
class Allocate : public TscopeOperation
{
public:
   Allocate(const string& subarray) 
      : subarray_(subarray)
   {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->allocate(subarray_);
      return "";
   }

private:
   const string & subarray_;
};

const char * TscopeParameters::allocate(
   const string &subarray, 
   const string& tscopeName) 
{
   Allocate operation(subarray);

   return runSubarrayCmd(subarray, operation, tscopeName);
}

//----------------------------------
class Deallocate : public TscopeOperation
{
public:
   Deallocate(const string& subarray) 
      : subarray_(subarray)
   {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->deallocate(subarray_);
      return "";
   }

private:
   const string & subarray_;
};

const char * TscopeParameters::deallocate(
   const string &subarray, 
   const string& tscopeName) 
{
   Deallocate operation(subarray);

   return runSubarrayCmd(subarray, operation, tscopeName);
}

//----------------------------------

class BfStop : public TscopeOperation
{
public:
   BfStop() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerStop();
      return "";
   }
};

// beamformer stop
const char * TscopeParameters::bfstop(const string& tscopeName)
{
   BfStop operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class BfReset : public TscopeOperation
{
public:
   BfReset() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerReset();
      return "";
   }
};

// beamformer reset
const char * TscopeParameters::bfreset(const string& tscopeName)
{
   BfReset operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class BfInit : public TscopeOperation
{
public:
   BfInit() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerInit();
      return "";
   }
};

// beamformer init
const char * TscopeParameters::bfinit(const string& tscopeName)
{
   BfInit operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------

class BfPoint : public TscopeOperation
{
public:
   BfPoint() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerPoint();
      return "";
   }
};

// beamformer point
const char * TscopeParameters::bfpoint(const string& tscopeName)
{
   BfPoint operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------

class BfClearCoords : public TscopeOperation
{
public:
   BfClearCoords() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerClearCoords();
      return "";
   }
};

// beamformer clearcoords
const char * TscopeParameters::bfclearcoords(const string& tscopeName)
{
   BfClearCoords operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------

class BfClearAnts : public TscopeOperation
{
public:
   BfClearAnts() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerClearAnts();
      return "";
   }
};

// beamformer clearants
const char * TscopeParameters::bfclearants(const string& tscopeName)
{
   BfClearAnts operation;

   internal_->outString = operation.runThroughTscopeList(tscopeName, getSite());
   return runCmd(operation, tscopeName);
}

//----------------------------------
class AssignAnts : public TscopeOperation
{
public:
   AssignAnts(const TscopeAssignSubarray & assignSub)
      : assignSub_(assignSub)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->assignSubarray(assignSub_);
      return "";
   }

private:
   const TscopeAssignSubarray & assignSub_;

};

/*
  Define the subarray of antennas that make up a beam.
 */
const char * TscopeParameters::assign(
   const string& beamName, const string& subarray, const string& tscopeName)
{
   const string methodName("assign");
   stringstream strm;

   string beamNameUppercase(SseUtil::strToUpper(beamName));
   TscopeBeam beam = SseTscopeMsg::nameToBeam(beamNameUppercase);
   if (beam == TSCOPE_INVALID_BEAM)
   {
      strm << "Error " << getCommand() << " " << methodName
           << ": invalid beam name: " << beamName;
      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }

   auto_ptr<TscopeAssignSubarray> assignSub(new TscopeAssignSubarray());
   assignSub->beam = beam;

   string errorText;
   if (!SseCommUtil::validAtaSubarray(subarray, errorText))
   {
      strm << errorText;
      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }

   SseUtil::strMaxCpy(assignSub->subarray, subarray.c_str(),
                      MAX_TEXT_STRING);

   AssignAnts operation(*assignSub);
   return runCmd(operation, tscopeName);
}

//----------------------------------

class Monitor : public TscopeOperation
{
public:
   Monitor(int periodSecs)
      : periodSecs_(periodSecs)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->monitor(periodSecs_);
      return "";
   }
private:

   int periodSecs_;
};

const char * TscopeParameters::monitor(int periodSecs, const string& tscopeName)
{
   Monitor operation(periodSecs);
   return runCmd(operation, tscopeName);
}

//----------------------------------


class PointSubarray : public TscopeOperation
{
public:
   PointSubarray(const TscopeSubarrayCoords & coords)
      : coords_(coords)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->pointSubarray(coords_);
      return "";
   }
private:

   const TscopeSubarrayCoords & coords_;
};


// point subarray 
const char * TscopeParameters::point(
   const string& subarray,
   const string& coordSystem,
   double coord1,
   double coord2,
   const string& tscopeName)
{
   const string methodName("point");
   const string coordSysJ2000("j2000");
   const string coordSysJ2000Deg("j2000deg");
   const string coordSysAzEl("azel");
   const string coordSysGal("gal");

   stringstream strm;  // for status return
   auto_ptr<TscopeSubarrayCoords> request(new TscopeSubarrayCoords()); 

   string errorText;
   if (!SseCommUtil::validAtaSubarray(subarray, errorText))
   {
      strm << errorText;
      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }
   
   SseUtil::strMaxCpy(request->subarray, subarray.c_str(), MAX_TEXT_STRING);

   if (SseUtil::strCaseEqual(coordSystem, coordSysAzEl))
   {
      request->pointing.coordSys = TscopePointing::AZEL;
      request->pointing.azDeg = coord1;
      request->pointing.elDeg = coord2;
   }
   else if (SseUtil::strCaseEqual(coordSystem, coordSysJ2000))
   {
      request->pointing.coordSys = TscopePointing::J2000;
      request->pointing.raHours = coord1;
      request->pointing.decDeg = coord2;

      if (!internal_->validPointRequestRaHours(strm, request->pointing.raHours) ||
          !internal_->validPointRequestDecDeg(strm, request->pointing.decDeg))
      {
	 internal_->outString = strm.str();
	 return(internal_->outString.c_str());
      }
   }
   else if (SseUtil::strCaseEqual(coordSystem, coordSysJ2000Deg))
   {
      double & raDeg = coord1;
      double & decDeg = coord2;

      if (!internal_->validPointRequestRaDeg(strm, raDeg) ||
	  !internal_->validPointRequestDecDeg(strm, decDeg))
      {
	 internal_->outString = strm.str();
	 return(internal_->outString.c_str());
      }

      const double DegPerHour(15.0);
      request->pointing.coordSys = TscopePointing::J2000;
      request->pointing.raHours = raDeg / DegPerHour;
      request->pointing.decDeg = decDeg;

      stringstream pointStrm;
      pointStrm.precision(9);   // show N places after the decimal
      pointStrm.setf(std::ios::fixed);  // show all decimal places up to precision

      pointStrm << "sending point request:  RA = "
		<< raDeg << " deg "
		<< "(" << request->pointing.raHours << " hours), "
		<< "Dec = " << request->pointing.decDeg << " deg " << endl;
      
      strm << pointStrm.str();

   }
   else if (SseUtil::strCaseEqual(coordSystem, coordSysGal))
   {
      request->pointing.coordSys = TscopePointing::GAL;
      request->pointing.galLongDeg = coord1;
      request->pointing.galLatDeg = coord2;
   } 
   else
   {
      strm << "Error " << getCommand() << " " << methodName 
	   << ": unsupported coordinate system: " << coordSystem << endl
	   << "Supported coord systems: "
	   << coordSysAzEl << " "
	   << coordSysGal << " "
	   << coordSysJ2000 << " "
	   << coordSysJ2000Deg << endl;

      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }

   PointSubarray operation(*request);
   return runCmd(operation, tscopeName);
}

//----------------------------------


class BfSetCoords : public TscopeOperation
{
public:
   BfSetCoords(const TscopeBeamCoords & coords)
      : coords_(coords)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerSetBeamCoords(coords_);
      return "";
   }
private:

   const TscopeBeamCoords & coords_;
};

const char * TscopeParameters::bfsetcoords(
   const string& beamName,
   const string& coordSystem,
   double coord1,
   double coord2,
   const string& tscopeName)
{
   const string methodName("bfsetcoords");
   const string coordSysJ2000("j2000");
   const string coordSysJ2000Deg("j2000deg");
   const string coordSysAzEl("azel");
   const string coordSysGal("gal");

   stringstream strm;  // for status return
   auto_ptr<TscopeBeamCoords> request(new TscopeBeamCoords());

   string beamNameUppercase(SseUtil::strToUpper(beamName));
   TscopeBeam beam = SseTscopeMsg::nameToBeam(beamNameUppercase);
   if (beam == TSCOPE_INVALID_BEAM)
   {
      strm << "Error " << getCommand() << " " << methodName
           << ": invalid beam name: " << beamName;
      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }
   
   request->beam = beam;

   if (SseUtil::strCaseEqual(coordSystem, coordSysAzEl))
   {
      request->pointing.coordSys = TscopePointing::AZEL;
      request->pointing.azDeg = coord1;
      request->pointing.elDeg = coord2;
   }
   else if (SseUtil::strCaseEqual(coordSystem, coordSysJ2000))
   {
      request->pointing.coordSys = TscopePointing::J2000;
      request->pointing.raHours = coord1;
      request->pointing.decDeg = coord2;

      if (!internal_->validPointRequestRaHours(strm, request->pointing.raHours) ||
	 !internal_->validPointRequestDecDeg(strm, request->pointing.decDeg))
      {
	 internal_->outString = strm.str();
	 return(internal_->outString.c_str());
      }
   }
   else if (SseUtil::strCaseEqual(coordSystem, coordSysJ2000Deg))
   {
      double & raDeg = coord1;
      double & decDeg = coord2;

      if (!internal_->validPointRequestRaDeg(strm, raDeg) ||
	  !internal_->validPointRequestDecDeg(strm, decDeg))
      {
	 internal_->outString = strm.str();
	 return(internal_->outString.c_str());
      }

      const double DegPerHour(15.0);
      request->pointing.coordSys = TscopePointing::J2000;
      request->pointing.raHours = raDeg / DegPerHour;
      request->pointing.decDeg = decDeg;

      stringstream pointStrm;
      pointStrm.precision(9);   // show N places after the decimal
      pointStrm.setf(std::ios::fixed);  // show all decimal places up to precision

      pointStrm << "sending beam set coords request:  RA = "
		<< raDeg << " deg "
		<< "(" << request->pointing.raHours << " hours), "
		<< "Dec = " << request->pointing.decDeg << " deg " << endl;
      
      strm << pointStrm.str();

   }
   else if (SseUtil::strCaseEqual(coordSystem, coordSysGal))
   {
      request->pointing.coordSys = TscopePointing::GAL;
      request->pointing.galLongDeg = coord1;
      request->pointing.galLatDeg = coord2;
   } 
   else
   {
      strm << "Error " << getCommand() << " " << methodName 
	   << ": unsupported coordinate system: " << coordSystem << endl
	   << "Supported coord systems: "
	   << coordSysAzEl << " "
	   << coordSysGal << " "
	   << coordSysJ2000 << " "
	   << coordSysJ2000Deg << endl;

      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }

   BfSetCoords operation(*request);
   return runCmd(operation, tscopeName);
}

//----------------------------------

class BfCal : public TscopeOperation
{
public:
   BfCal(const TscopeCalRequest & request)
      : request_(request)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->beamformerCal(request_);
      return "";
   }
private:

   const TscopeCalRequest & request_;
};

// bfcal <delay|phase|freq> integrate <secs> cycles <count> iterate <on|off>

const char * TscopeParameters::bfcal(
   const string& calType,  // delay, phase, freq
   const string& integrateKeyword,
   int integrateSecs,
   const string& cyclesKeyword,
   int numCycles,
   const string& iterateKeyword,
   int calIterations,
   const string& tscopeName)
{
   const string methodName("bfcal");
   stringstream strm;  // for status return
   auto_ptr<TscopeCalRequest> request(new TscopeCalRequest());

   const string calDelayType("delay");
   const string calPhaseType("phase");
   const string calFreqType("freq");

   if (calType == calDelayType)
   {
      request->calType = TscopeCalRequest::DELAY; 
   }
   else if (calType == calPhaseType)
   {
      request->calType = TscopeCalRequest::PHASE; 
   }
   else if (calType == calFreqType)
   {
      request->calType = TscopeCalRequest::FREQ; 
   }
   else
   {
      strm << "Error: " << getCommand() << " " << methodName 
           << " cal type must be one of: " 
           << calDelayType << " " << calPhaseType << " " 
           << calFreqType;
         
      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }

   request->integrateSecs = integrateSecs;
   request->numCycles = numCycles;
   request->calIterations = calIterations;

   BfCal operation(*request);
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Names : public TscopeOperation
{
public:
   Names() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {	
      return proxy->getName() + " ";
   }
};

const char * TscopeParameters::names()
{
   Names operation;

   string tscopeName("all");
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Intrin : public TscopeOperation
{
public:
   Intrin() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      stringstream strm;
      
      strm << proxy->getIntrinsics();
      return strm.str();
   }
};

const char* TscopeParameters::intrin(const string& tscopeName) 
{
   Intrin operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Reset : public TscopeOperation
{
public:
   Reset() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->reset();
      return "";
   }
};


const char * TscopeParameters::reset(const string& tscopeName) 
{
   Reset operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class ResetSocket : public TscopeOperation
{
public:
   ResetSocket() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->resetSocket();
      return "";
   }
};


const char * TscopeParameters::resetsocket(const string& tscopeName) 
{
   ResetSocket operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Tune : public TscopeOperation
{
public:
   Tune(const TscopeTuneRequest & request)
      : request_(request)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->tune(request_);
      return "";
   }

private:
   const TscopeTuneRequest & request_;

};


const char * TscopeParameters::tune(
   const string& tuningName,
   double skyFreqMhz, 
   const string& tscopeName)
{
   const string methodName("tune");

   // verify tuning name
   string tuningNameUppercase(SseUtil::strToUpper(tuningName));
   TscopeTuning tuning = SseTscopeMsg::nameToTuning(tuningNameUppercase);
   if (tuning == TSCOPE_INVALID_TUNING)
   {
      stringstream strm;
      strm << "Error " << getCommand() << " " << methodName
	   << ": invalid tuning name: " << tuningName;
      internal_->outString = strm.str();
      return(internal_->outString.c_str());
   }

   auto_ptr<TscopeTuneRequest> request(new TscopeTuneRequest());
   request->tuning = tuning;
   request->skyFreqMhz = skyFreqMhz;

   Tune operation(*request);
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Focus: public TscopeOperation
{
public:
   Focus(const string& subarray, double skyFreqMhz)
      : subarray_(subarray), skyFreqMhz_(skyFreqMhz)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->zfocus(subarray_, skyFreqMhz_);
      return "";
   }

private:
   const string & subarray_;
   double skyFreqMhz_;
};


const char * TscopeParameters::zfocus(
   const string &subarray,
   double skyFreqMhz, 
   const string& tscopeName)
{
   Focus operation(subarray, skyFreqMhz);
   return runSubarrayCmd(subarray, operation, tscopeName);
}

//----------------------------------
class ReqStat : public TscopeOperation
{
public:
   ReqStat() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
       proxy->requestStatusUpdate();
       return "";
   }
};

const char * TscopeParameters::reqstat(const string& tscopeName)
{
   ReqStat operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Status : public TscopeOperation
{
public:
   Status() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {	
      stringstream strm;

      // returns cached tscope status
      strm << proxy->getName() << endl;
      strm << proxy->getStatus();

      return strm.str();
   }
};

const char * TscopeParameters::status(const string& tscopeName) 
{
   Status operation;
   return runCmd(operation, tscopeName);
}


// combined commands for easy telescope startup

//----------------------------------
class SetUp : public TscopeOperation
{
public:
   SetUp() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->connect();

      return "";
   }
};

const char * TscopeParameters::setup(const string& tscopeName) 
{
   SetUp operation;
   return runCmd(operation, tscopeName);
}


//----------------------------------
class CleanUp : public TscopeOperation
{
public:
   CleanUp() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->disconnect();
         
      return "";
   }
};

const char * TscopeParameters::cleanup(const string& tscopeName) 
{
   CleanUp operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Shutdown : public TscopeOperation
{
public:
   Shutdown() {}

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->disconnect();  // break connection to ant server

      if (componentControlImmedCmdsGlobal.isComponentUnderControl(proxy->getName()))
      {
         // Let the component controller perform a shutdown so that
         // the component will not be restarted.
         componentControlImmedCmdsGlobal.shutdown(proxy->getName());
      }
      else
      {
         // Let the proxy handle shutdown on its own.
         proxy->shutdown();
      }

      return "";
   }

};

const char * TscopeParameters::shutdown(const string& tscopeName) 
{
   Shutdown operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Restart : public TscopeOperation
{
public:
   Restart() {}

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
	 proxy->disconnect(); // break connection to ant server
	  
	 if (componentControlImmedCmdsGlobal.isComponentUnderControl(proxy->getName()))
	 {
	    componentControlImmedCmdsGlobal.restart(proxy->getName());
	 }
	 else
	 {
	    // Proxy can't do this on its own
	 }

      return "";
   }

};

const char * TscopeParameters::restart(const string& tscopeName) 
{
   Restart operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class Stop: public TscopeOperation
{
public:
   Stop(const string& subarray)
      : subarray_(subarray)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->stop(subarray_);
      return "";
   }

private:
   const string & subarray_;
};


const char * TscopeParameters::stop(
   const string & subarray,
   const string & tscopeName)
{
   Stop operation(subarray);
   return runSubarrayCmd(subarray, operation, tscopeName);
}

//----------------------------------
class Stow: public TscopeOperation
{
public:
   Stow(const string& subarray)
      : subarray_(subarray)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->stow(subarray_);
      return "";
   }

private:
   const string & subarray_;
};


const char * TscopeParameters::stow(
   const string & subarray,
   const string & tscopeName)
{
   Stow operation(subarray);
   return runSubarrayCmd(subarray, operation, tscopeName);
}

//----------------------------------
class Wrap: public TscopeOperation
{
public:
   Wrap(const string& subarray, int wrapNumber)
      : subarray_(subarray), wrapNumber_(wrapNumber)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->wrap(subarray_, wrapNumber_);
      return "";
   }

private:
   const string & subarray_;
   int wrapNumber_;
};


const char * TscopeParameters::wrap(
   const string &subarray,
   int wrapNumber, 
   const string& tscopeName)
{
   Wrap operation(subarray, wrapNumber);
   return runSubarrayCmd(subarray, operation, tscopeName);
}

//----------------------------------
class Connect : public TscopeOperation
{
public:
   Connect() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->connect();
      return "";
   }
};


const char * TscopeParameters::connect(const string& tscopeName) 
{
   Connect operation;
   return runCmd(operation, tscopeName);
}


//----------------------------------
class Disconnect : public TscopeOperation
{
public:
   Disconnect() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->disconnect();
      return "";
   }
};


const char * TscopeParameters::disconnect(const string& tscopeName) 
{
   Disconnect operation;
   return runCmd(operation, tscopeName);
}


//----------------------------------
class Sim : public TscopeOperation
{
public:
   Sim() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->simulate();
      return "";
   }
};


const char * TscopeParameters::sim(const string& tscopeName) 
{
   Sim operation;
   return runCmd(operation, tscopeName);
}

//----------------------------------
class UnSim : public TscopeOperation
{
public:
   UnSim() {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->unsimulate();
      return "";
   }
};


const char * TscopeParameters::unsim(const string& tscopeName) 
{
   UnSim operation;
   return runCmd(operation, tscopeName);
}


//----------------------------------
class AutoselectAnts : public TscopeOperation
{
public:
   AutoselectAnts(const string & bflist)
      : bflist_(bflist)
   {}

protected:
   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->antgroupAutoselect(bflist_);
      return "";
   }

private:
   const string & bflist_;
};

const char * TscopeParameters::autoselectants(const string & bflist,
		                                      const string & tscopeName)
{
   AutoselectAnts operation(bflist);
   return runCmd(operation, tscopeName);
}

//----------------------------------
/*
  "Backdoor" that lets you send a raw command directly to the
  backend server.
*/
class Send: public TscopeOperation
{
public:
   Send(const string& cmdWithArgs)
      : cmdWithArgs_(cmdWithArgs)
   {
   }

protected:

   virtual string callOperation(TscopeProxy *proxy)
   {
      proxy->sendBackendCommand(cmdWithArgs_);
      return "";
   }

private:
   const string & cmdWithArgs_;
};


const char * TscopeParameters::send(
   const string & tscopeName,
   const string & cmd,
   const string & cmdArg1,
   const string & cmdArg2,
   const string & cmdArg3,
   const string & cmdArg4,
   const string & cmdArg5,
   const string & cmdArg6,
   const string & cmdArg7,
   const string & cmdArg8)
{
   string methodName("send");
   stringstream strm;

   string cmdWithArgs = cmd + " "
      + cmdArg1 + " " + cmdArg2 + " " 
      + cmdArg3 + " " + cmdArg4 + " " 
      + cmdArg5 + " " + cmdArg6 + " " 
      + cmdArg7 + " " + cmdArg8; 

   Send operation(cmdWithArgs);
   return runCmd(operation, tscopeName);
}


// --------------------------------------------
// Misc. access methods

double TscopeParameters::getBasebandTuneOffsetMhz() const
{
   return internal_->basebandTuneOffsetMhz.getCurrent();
}

bool TscopeParameters::setBasebandTuneOffsetMhz(double offsetMhz)
{
   return internal_->basebandTuneOffsetMhz.setCurrent(offsetMhz);
}

double TscopeParameters::getBasebandCenterTuneOffsetMhz() const
{
   return internal_->basebandCenterTuneOffsetMhz.getCurrent();
}

double TscopeParameters::getPrimaryFovAtOneGhzDeg() const
{
   return internal_->primaryFovAtOneGhzDeg.getCurrent();
}

double TscopeParameters::getBeamsizeAtOneGhzArcSec() const
{
   return internal_->beamsizeAtOneGhzArcSec.getCurrent();
}


bool TscopeParameters::setTuningSkyFreqMhz(const string &tuningName,
					   double skyFreqMhz)
{
   return findTuningParamForTuningName(tuningName)->setCurrent(skyFreqMhz);
}

double TscopeParameters::getTuningSkyFreqMhz(const string &tuningName) const
{
   return findTuningParamForTuningName(tuningName)->getCurrent();

}

TscopeCalRequest TscopeParameters::getCalRequest() const
{
   TscopeCalRequest request;

   request.calType = SseTscopeMsg::stringToCalType(internal_->calType.getCurrent());
   request.integrateSecs = internal_->calTimeSecs.getCurrent();
   request.numCycles = internal_->calNumCycles.getCurrent();
   request.calIterations = internal_->calIterations.getCurrent();
      
   return request;
}

RangeParameter<double> * TscopeParameters::
findTuningParamForTuningName(const string & tuningName) const
{
   string methodName("findTuningParamForTuningName");

   // go through each tuning param in turn, looking for a name match

   typedef list<RangeParameter<double> *> RangeParamList;
   RangeParamList tuningParamList;

   tuningParamList.push_back(&internal_->tuningASkyFreqMhz);
   tuningParamList.push_back(&internal_->tuningBSkyFreqMhz);
   tuningParamList.push_back(&internal_->tuningCSkyFreqMhz);
   tuningParamList.push_back(&internal_->tuningDSkyFreqMhz);

   for (RangeParamList::iterator it = tuningParamList.begin(); 
	it != tuningParamList.end(); ++it)
   {
      RangeParameter<double> *tuningParam = *it; 
      if (tuningName == tuningParam->getName())
      {
	 return tuningParam;
      }
   }

   // Not found:
   stringstream strm;
   strm << "TscopeParameters::" << methodName
	<< ": could not find parameter '"
	<< tuningName << "'\n";
    
   throw SseException(strm.str(), __FILE__, __LINE__,
		      SSE_MSG_INVALID_PARMS, SEVERITY_ERROR);
    
}


//----------------------------------

void TscopeParametersInternal::setCommand(const string & command)
{
   this->command = command;
}

bool TscopeParametersInternal::validPointRequestRaHours(ostream &errorStrm, double raHours)
{
   const double MinRaHours(0.0);
   const double MaxRaHours(24.0);
   if (raHours < MinRaHours || raHours > MaxRaHours)
   {
      errorStrm << "Error " << command 
		<< ": requested RA " << raHours
		<< " hours is out of range: " 
		<< MinRaHours << " -> " << MaxRaHours 
		<< " hours " << endl;
      
      return false;
   }

   return true;
}

bool TscopeParametersInternal::validPointRequestRaDeg(ostream &errorStrm, double raDeg)
{
   const double MinRaDeg(0.0);
   const double MaxRaDeg(360.0);
   if (raDeg < MinRaDeg || raDeg > MaxRaDeg)
   {
      errorStrm << "Error " << command
		<< ": requested RA " << raDeg
		<< " deg is out of range: " 
		<< MinRaDeg << " -> " << MaxRaDeg 
		<< " deg " <<  endl;

      return false;
   }

   return true;
}

bool TscopeParametersInternal::validPointRequestDecDeg(ostream &errorStrm, double decDeg)
{
   const double MinDecDeg(AtaInformation::AtaMinDecLimitDeg);
   const double MaxDecDeg(90.0);
   if (decDeg < MinDecDeg || decDeg > MaxDecDeg)
   {
      errorStrm << "Error " << command
		<< ": requested Dec " << decDeg 
		<< " deg is out of range: "
		<< MinDecDeg << " -> " << MaxDecDeg 
		<< " deg " <<  endl;
      
      return false;
   }

   return true;
}

const string TscopeParameters::getCalType() const
{
   return internal_->calType.getCurrent();
}

bool TscopeParameters::setCalType(const string & calTypeName)
{
   return internal_->calType.setCurrent(calTypeName);
}

int TscopeParameters::getCalIterations() const
{
   return internal_->calIterations.getCurrent();
}

bool TscopeParameters::setCalIterations(int calIterations)
{
   return internal_->calIterations.setCurrent(calIterations);
}

int TscopeParameters::getCalTimeSecs() const
{
   return internal_->calTimeSecs.getCurrent();
}

bool TscopeParameters::setCalTimeSecs(int timeSecs)
{
   return internal_->calTimeSecs.setCurrent(timeSecs);
}

int TscopeParameters::getCalNumCycles() const
{
   return internal_->calNumCycles.getCurrent();
}

bool TscopeParameters::setCalNumCycles(int numCycles)
{
   return internal_->calNumCycles.setCurrent(numCycles);
}

double TscopeParameters::getSiteLatNorthDeg()
{
   return internal_->siteLatNorthDeg.getCurrent();
}

double TscopeParameters::getSiteLongWestDeg()
{
   return internal_->siteLongWestDeg.getCurrent();
}

double TscopeParameters::getSiteHorizDeg()
{
   return internal_->siteHorizDeg.getCurrent();
}

string TscopeParameters::getSiteName()
{
   return internal_->siteName.getCurrent();
}

string TscopeParameters::getXpolAnts()
{
   return internal_->xpolAnts.getCurrent();
}

string TscopeParameters::getYpolAnts()
{
   return internal_->ypolAnts.getCurrent();
}

string TscopeParameters::getPrimaryAnts()
{
   return internal_->primaryAnts.getCurrent();
}

bool TscopeParameters::isAntListSourceParam()
{
   Assert (internal_->antlistSource.getCurrent() == AntListSourceParam
           || internal_->antlistSource.getCurrent() == AntListSourceAntGroup);

   return (internal_->antlistSource.getCurrent() == AntListSourceParam);
}
