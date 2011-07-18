/*******************************************************************************

 File:    NssParameters.cpp
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


#include "NssParameters.h"
#include "Site.h"
#include "SseArchive.h"
#include "IfcImmedCmds.h"
#include "IfcParameters.h"
#include "TscopeParameters.h"
#include "TestSigParameters.h"
#include "TestSigImmedCmds.h"
#include "DxParameters.h"
#include "DxArchiverParameters.h"
#include "ChannelizerParameters.h"
#include "ActParameters.h"
#include "SchedulerParameters.h"
#include "DbParameters.h"
#include "ComponentControlImmedCmds.h"
#include "ExpectedNssComponentsTree.h"
#include "Scheduler.h"
#include <ace/Reactor.h>

// single global instances of activity parameters classes

IfcParameters ifc1Global(Ifc1CommandName);
IfcParameters ifc2Global(Ifc2CommandName);
IfcParameters ifc3Global(Ifc3CommandName);
TscopeParameters tscopeGlobal(TscopeCommandName);
TestSigParameters tsig1Global(Tsig1CommandName);
TestSigParameters tsig2Global(Tsig2CommandName);
TestSigParameters tsig3Global(Tsig3CommandName);
DxParameters dxGlobal(DxCommandName);
DxArchiverParameters archGlobal(DxArchiverCommandName);
ChannelizerParameters chanGlobal(ChannelizerCommandName);
ActParameters actGlobal(ActParamCommandName);
SchedulerParameters schedGlobal(SchedulerParamCommandName);
DbParameters dbGlobal(DbParamCommandName);


// These do not contain any parameters (immediate commands only)
// but it's convenient to use it here for unified help, etc.
IfcImmedCmds ifcGlobal(IfcCommandName);
TestSigImmedCmds tsigGlobal(TsigCommandName);
ComponentControlImmedCmds componentControlImmedCmdsGlobal(ComponentControlName);

VerboseForSwig verboseGlobal;
static string ReplyString;

NssParameters paraGlobal(&ifcGlobal, &ifc1Global, &ifc2Global, &ifc3Global,
			 &tscopeGlobal, &tsigGlobal, &tsig1Global, 
			 &tsig2Global, &tsig3Global, &dxGlobal, &archGlobal, 
			 &chanGlobal, &actGlobal, &schedGlobal, &dbGlobal,
			 &componentControlImmedCmdsGlobal);

NssParameters::NssParameters(
   IfcImmedCmds * ifcImmedCmds,
   IfcParameters* ifc1Parameters,
   IfcParameters* ifc2Parameters,
   IfcParameters* ifc3Parameters,
   TscopeParameters* tscopeParameters, 
   TestSigImmedCmds * testSigImmedCmds,
   TestSigParameters* testSig1Parameters, 
   TestSigParameters* testSig2Parameters, 
   TestSigParameters* testSig3Parameters, 
   DxParameters *dxParameters,
   DxArchiverParameters *dxArchiverParameters,
   ChannelizerParameters *channelizerParameters,
   ActParameters *actParameters,
   SchedulerParameters* schedulerParameters,
   DbParameters* db,
   ComponentControlImmedCmds *componentControlImmedCmds)
   : 
   ifc_(ifcImmedCmds), 
   ifc1_(ifc1Parameters), 
   ifc2_(ifc2Parameters), 
   ifc3_(ifc3Parameters), 
   tscope_(tscopeParameters),
   tsig_(testSigImmedCmds), 
   tsig1_(testSig1Parameters), 
   tsig2_(testSig2Parameters), 
   tsig3_(testSig3Parameters), 
   dx_(dxParameters),
   arch_(dxArchiverParameters),
   chan_(channelizerParameters), 
   act_(actParameters),
   sched_(schedulerParameters),
   db_(db),
   componentControl_(componentControlImmedCmds),
   site_(0),
   cleanup_(false)
{
}

NssParameters::NssParameters(const NssParameters& rhs):
   ifc_(new IfcImmedCmds(*rhs.ifc_)),
   ifc1_(new IfcParameters(*rhs.ifc1_)),
   ifc2_(new IfcParameters(*rhs.ifc2_)),
   ifc3_(new IfcParameters(*rhs.ifc3_)),
   tscope_(new TscopeParameters(*rhs.tscope_)),
   tsig_(new TestSigImmedCmds(*rhs.tsig_)),
   tsig1_(new TestSigParameters(*rhs.tsig1_)),
   tsig2_(new TestSigParameters(*rhs.tsig2_)),
   tsig3_(new TestSigParameters(*rhs.tsig3_)),
   dx_(new DxParameters(*rhs.dx_)),
   arch_(new DxArchiverParameters(*rhs.arch_)),
   chan_(new ChannelizerParameters(*rhs.chan_)),
   act_(new ActParameters(*rhs.act_)),
   sched_(new SchedulerParameters(*rhs.sched_)),
   db_(new DbParameters(*rhs.db_)),
   componentControl_(new ComponentControlImmedCmds(*rhs.componentControl_)),
   site_(rhs.site_),
   cleanup_(true)
{
}

NssParameters& NssParameters::operator=(const NssParameters& rhs)
{
   if (this == &rhs) {
      return *this;
   }
  
   delete ifc_;
   ifc_ = new IfcImmedCmds(*rhs.ifc_);

   delete ifc1_;
   ifc1_ = new IfcParameters(*rhs.ifc1_);

   delete ifc2_;
   ifc2_ = new IfcParameters(*rhs.ifc2_);

   delete ifc3_;
   ifc3_ = new IfcParameters(*rhs.ifc3_);

   delete tscope_;
   tscope_ = new TscopeParameters(*rhs.tscope_);

   delete tsig_;
   tsig_ = new TestSigImmedCmds(*rhs.tsig_);

   delete tsig1_;
   tsig1_ = new TestSigParameters(*rhs.tsig1_);

   delete tsig2_;
   tsig2_ = new TestSigParameters(*rhs.tsig2_);

   delete tsig3_;
   tsig3_ = new TestSigParameters(*rhs.tsig3_);

   delete dx_;
   dx_ = new DxParameters(*rhs.dx_);

   delete arch_;
   arch_ = new DxArchiverParameters(*rhs.arch_);

   delete chan_;
   chan_ = new ChannelizerParameters(*rhs.chan_);

   delete act_;
   act_ = new ActParameters(*rhs.act_);
  
   delete sched_;
   sched_ = new SchedulerParameters(*rhs.sched_);

   delete db_;
   db_ = new DbParameters(*rhs.db_);

   delete componentControl_;
   componentControl_ = new ComponentControlImmedCmds(*rhs.componentControl_);

   site_ = rhs.site_;
   cleanup_ = true;
  
   return *this;
}


NssParameters::~NssParameters()
{
   if (cleanup_)
   {
      delete ifc_;
      delete ifc1_;
      delete ifc2_;
      delete ifc3_;
      delete tscope_;
      delete tsig_;
      delete tsig1_;
      delete tsig2_;
      delete tsig3_;
      delete dx_;
      delete arch_;
      delete chan_;
      delete act_;
      delete sched_;
      delete db_;
      delete componentControl_;

      // do not delete site_
      // it is just a copy of a pointer
   }
}

bool NssParameters::isValid() const
{
   if (!ifc1_->isValid()) return(false);
   if (!ifc2_->isValid()) return(false);
   if (!ifc3_->isValid()) return(false);
   if (!tscope_->isValid()) return(false);
   if (!tsig1_->isValid()) return(false);
   if (!tsig2_->isValid()) return(false);
   if (!tsig3_->isValid()) return(false);
   if (!dx_->isValid()) return(false);
   if (!arch_->isValid()) return(false);
   if (!chan_->isValid()) return(false);
   if (!act_->isValid()) return(false);
   if (!sched_->isValid()) return (false);
   if (!db_->isValid()) return (false);
   return(true);
}

const char * NssParameters::setDefault() 
{
   stringstream strm;

   strm << ifc1_->setDefault()
	<< ifc2_->setDefault()
	<< ifc3_->setDefault()
	<< tscope_->setDefault()
	<< tsig1_->setDefault()
	<< tsig2_->setDefault()
	<< tsig3_->setDefault()
	<< dx_->setDefault()
	<< arch_->setDefault()
	<< chan_->setDefault()
	<< act_->setDefault()
	<< sched_->setDefault()
	<< db_->setDefault();
   
   outString_ = strm.str();
   return outString_.c_str();
}

const char * NssParameters::show() const
{
   stringstream strm;

   strm << act_->show("all") << endl
	<< arch_->show("all") << endl
	<< chan_->show("all") << endl
	<< db_->show("all") << endl
	<< dx_->show("all") << endl
	<< sched_->show("all") << endl
	<< tscope_->show("all") << endl;

   outString_ = strm.str();
   return outString_.c_str();
   
}

const char * NssParameters::names() const
{
   stringstream strm;

   strm << "ifc names: "
	<< ifc_->names() << endl

	<< "tscope names: "
	<< tscope_->names() << endl

	<< "tsig names: "
	<< tsig_->names() << endl

	<< "dx names: "
	<< dx_->names() << endl

	<< "archiver names: "
	<< arch_->names() << endl

	<< "channelizer names: "
	<< chan_->names();


   outString_ = strm.str();
   return outString_.c_str();
}

void NssParameters::help(ostream& os) const {
   os << "NssParameters help:" << endl << endl;
   act_->help(os); os << endl;
   arch_->help(os); os << endl;
   chan_->help(os); os << endl;
   componentControl_->help(os); os << endl;
   db_->help(os); os << endl;
   dx_->help(os); os << endl;
   sched_->help(os); os << endl;
   tscope_->help(os);  os << endl;

   printMiscCommandsHelp(os); os << endl;
}

const char * NssParameters::save(const string& filename) const
{
   stringstream strm;

   strm << act_->save(filename)
	<< arch_->save(filename)
	<< chan_->save(filename)
	<< db_->save(filename)
	<< ifc1_->save(filename)
	<< ifc2_->save(filename)
	<< ifc3_->save(filename)
	<< dx_->save(filename)
	<< sched_->save(filename)
	<< tscope_->save(filename)
	<< tsig1_->save(filename)
	<< tsig2_->save(filename)
	<< tsig3_->save(filename);

   outString_ = strm.str();
   return outString_.c_str();
}


void NssParameters::printHelpDescription(ostream &os) const
{
   os << "SSE/seeker help\n"
      << "===============\n"
      << "help all - displays help for all subsystems\n"
      << "help <subsystem>  - displays help for the given subsystem:\n"
      << "   act (activity)\n" 
      << "   arch (archiver)\n" 
      << "   channel (channelizer)\n" 
      << "   control (component control)\n"
      << "   db (database)\n" 
      << "   dx (detection module)\n" 
      << "   sched (scheduler)\n" 
      << "   tscope (telescope & RF control)\n" 

      << "help misc - displays the misc. commands\n"

      << endl
      << "The subsystem control is split into two parts, \n"
      << "Parameter Commands and Immediate commands:\n"
      << endl

      << "help param - overview of subsystem parameter control\n"
      << "help immed - overview of subsystem immediate commands\n"

      << endl;
}

void NssParameters::printHelpParametersOverview(ostream &os) const
{    
   os << "Subsystem Parameter Commands\n"
      << "=============================\n";

   os << "While the subsystems each have different parameters, they all\n" 
      << "share the same commands for setting, displaying, and\n" 
      << "otherwise manipulating their parameters.\n"
      << endl;
    
   os << "The parameter manipulation commands are:\n"
      << "<subsystem> <command with arguments, if any>\n"
      << endl;
   os << "where <command> is one of\n"
      << "   default                Set all parameters to their default values\n"
      << "   help                   Print this subsystem's help info\n"
      << "   isvalid                Verify that all subsystem parameters are valid\n"
      << "   save <filename>        Save subsystem parameters to file\n"
      << "   set <parameter> <value> [<current, default, min, max>=current]\n"
      << "                          Set the current, default, minimum or maximum\n"
      << "                           value of a parameter\n"
      << "   show [<parameter>=all] [<current, default, min, max>=current]\n"      
      << "                       Show all subsystem parameters\n"
      << endl;

   os << "Examples: \n"
	
      << " act set targetbeam1 5122    Sets the observing target to targetId 5122 on beam1\n" 
      << " dx set length 195   Sets the observation length to 195 seconds\n" 
      << " dx show             Displays current, default, min, and max values for all dx parameters\n"
      << " dx default          Restore the default dx parameter settings\n"
      << endl;

   os << "The parameter values that are changed in the user interface do not take\n"
      << "affect until the next 'start' activity command is issued. \n"
      << endl;
}

void NssParameters::printHelpImmediateCommandsOverview(ostream &os) const
{

   os << "Subsystem Immediate Commands\n" 
      << "============================\n"
      << "The general immediate command format is: \n"
      << "<subsystem> <command> [ < name | 'all' > ]\n" 
      << "These commands are sent to the subsystem immediately. The results of\n" 
      << "the commands will appear in the user interface window or a status\n" 
      << "window as soon they are available. \n"
      << endl;

   os << "Examples: \n"
      << " dx stop dx7  - stop the activities running on dx7\n"
      << " dx stop all  -  stop the activities on all the attached dxs\n"
      << endl;

   os << "Miscellaneous Commands\n"
      << "========================\n"
      << "These commands are not associated with a subsystem and the\n"
      << "formats vary. \n"
      << endl;

   os << "Examples: \n"
      << " start obs - start a new observation activity\n"
      << " stop - stop the currently running activities\n"
      << endl;
}

void NssParameters::printMiscCommandsHelp(ostream &os) const
{
   os << "  Misc commands:" << endl;
   os << "    exec <command> - execute an operating system command\n"
      << "    exit - exit sse/seeker\n"
      << "    expectedcomponents - display the 'expected NSS components' config file info\n"
      << "    followup <prev act id> - start a followup of a previous activity\n"
      << "    freeants - shorthand for 'start freeants'\n"
      << "    grid <prev act id> - start a grid pointing followup of a previous activity\n"
      << "    help [<subsystem> = 'all'] - display help information\n"
      << "    interfaceversions - display component interface versions\n"
      << "    log <message> - write message to the system log\n"
      << "    names - list names of all connected components\n"
      << "    para default - set all parameters to their default values\n"
      << "    quit - exit sse/seeker (alias for exit)\n"
      << "    reconfig [<filename>] - load components config from <filename>\n"
      << "    restore <filename> - load parameter settings from <filename>\n"
      << "    save <filename> - save parameter settings to <filename>\n"
      << "    sh <command> - use the unix shell to execute an operating system command\n"
      << "    show - show all parameter settings\n"
      << "    start <'tasks' | <task>[,<task>...]>- start selected tasks (i.e., strategies)\n"
      << "    status -  display status of all components\n"
      << "    stop - stop all activities\n"
      << "    wrapup - complete currently running activities & any followups, then stop\n"
      << "    targetidallbeams <id> - set the target id on all beams in act params\n"
      << "    tclerror - display last tcl error\n"
      << "    utc - display current UTC time\n"
      << "    verbose level <0=off, 1=some, 2=more> \n\t\tSet verbose level. Msgs go to log.\n"
      << "    verbose showlevel - display current verbose level\n"
      << "    version - display sse version number\n"
      ;

}

void NssParameters::setSite(Site *site) {
   site_ = site;
   ifc_->setSite(site);
   ifc1_->setSite(site);
   ifc2_->setSite(site);
   ifc3_->setSite(site);
   tscope_->setSite(site);
   tsig_->setSite(site);
   tsig1_->setSite(site);
   tsig2_->setSite(site);
   tsig3_->setSite(site);
   dx_->setSite(site);
   arch_->setSite(site);
   chan_->setSite(site);
   act_->setSite(site);
   sched_->setSite(site);
   db_->setSite(site);
   componentControl_->setSite(site);
}

Site * NssParameters::getSite() 
{
   return site_;
}

ostream& operator << (ostream &strm, const class NssParameters& nssParam) {
   strm << "NssParameters: " << endl
	<< *nssParam.act_ << endl
	<< *nssParam.arch_ << endl
	<< *nssParam.chan_ << endl
	<< *nssParam.db_ << endl
	<< *nssParam.ifc1_ << endl
	<< *nssParam.ifc2_ << endl
	<< *nssParam.ifc3_ << endl
	<< *nssParam.dx_ << endl
	<< *nssParam.sched_ << endl
	<< *nssParam.tscope_ << endl
	<< *nssParam.tsig1_ << endl
	<< *nssParam.tsig2_ << endl
	<< *nssParam.tsig3_ << endl
      ;
   return(strm);
}

const char *NssParameters::status() const
{
   stringstream strm;
   site_->printSystemStatus(strm);

   outString_ = strm.str();
   return outString_.c_str();
}


// restore from database to parameters
void NssParameters::restore(MYSQL* db, ActivityId_t activityId)
{
   // some parameters are not stored to the database
   // these parameters will not be restored

   dx_->restore(db, activityId);
   act_->restore(db, activityId);
   sched_->restore(db, activityId);

// TBD: store these in database?
  //  tscope_->restore(db, activityId);
  //   tsig_->restore(db, activityId);

  // TBD fix this for ifc1,2 & 3
  //  ifc_->restore(db, activityId);

  //  arch_->restore(db, activityId);
  //  chan_->restore(db, activityId);

   //  db_->restore(db, activityId);
}


// return the current utc time
const char *utc() 
{
   ReplyString = SseUtil::currentIsoDateTime();

   return ReplyString.c_str();
}

const char * help(const char *subsystemName)
{
   stringstream strm;

   string name(subsystemName);
   if (name == "help")
   {
      paraGlobal.printHelpDescription(strm);
   }
   else if (name == "param")
   {
      paraGlobal.printHelpParametersOverview(strm);
   }
   else if (name == "immed")
   {
      paraGlobal.printHelpImmediateCommandsOverview(strm);
   }
   else if (name == "all")
   { 
      paraGlobal.help(strm);
   }
   else if (name == dxGlobal.getCommand())
   { 
      dxGlobal.help(strm);
   }
   else if (name == archGlobal.getCommand())
   { 
      archGlobal.help(strm);
   }
   else if (name == chanGlobal.getCommand())
   { 
      chanGlobal.help(strm);
   }
   else if (name == componentControlImmedCmdsGlobal.getCommand())
   { 
      componentControlImmedCmdsGlobal.help(strm);
   }
   else if (name == ifcGlobal.getCommand())
   { 
      ifcGlobal.help(strm);
   }
   else if (name == ifc1Global.getCommand())
   { 
      ifc1Global.help(strm);
   }
   else if (name == ifc2Global.getCommand())
   { 
      ifc2Global.help(strm);
   }
   else if (name == ifc3Global.getCommand())
   { 
      ifc3Global.help(strm);
   }
   else if (name == tscopeGlobal.getCommand())
   { 
      tscopeGlobal.help(strm);
   }
   else if (name == tsigGlobal.getCommand())
   { 
      tsigGlobal.help(strm);
   }
   else if (name == tsig1Global.getCommand())
   { 
      tsig1Global.help(strm);
   }
   else if (name == tsig2Global.getCommand())
   { 
      tsig2Global.help(strm);
   }
   else if (name == tsig3Global.getCommand())
   { 
      tsig3Global.help(strm);
   }
   else if (name == actGlobal.getCommand())
   { 
      actGlobal.help(strm);
   }
   else if (name == schedGlobal.getCommand()){
      schedGlobal.help(strm);
   }
   else if (name == dbGlobal.getCommand()){
      dbGlobal.help(strm);
   }
   else if (name == "misc"){
      paraGlobal.printMiscCommandsHelp(strm);
   }
   else 
   {
      strm << "No help available for " << name << endl;
   }

   // Keep string around so tcl can grab it
   ReplyString = strm.str();

   return ReplyString.c_str();
}

void systemlog(const char *msg)
{
   SseArchive::SystemLog() << msg << endl;
}

// print the expected components information
const char * expectedcomponents()
{
   Site *site(paraGlobal.getSite());

   ExpectedNssComponentsTree *tree(site->getExpectedNssComponentsTree());

   stringstream strm;
   strm << *tree;

   ReplyString = strm.str();
   return ReplyString.c_str();
}


const char * reconfig(const char *expectedComponentsFilename) 
{
   if (Scheduler::instance()->isStrategyActive())
   {
      return "Cannot reconfig while strategy is active";
   }

   Site *site(paraGlobal.getSite());
   site->loadExpectedComponentsConfig(expectedComponentsFilename);

   site->updateSystemStatus();

   return "";
}


int VerboseForSwig::showlevel() const
{
   return Scheduler::instance()->getVerboseLevel();
}

int VerboseForSwig::level(int verboseLevel) const
{
   Scheduler::instance()->setVerboseLevel(verboseLevel);
   return verboseLevel;
}

// function executed when tcl exits
void swigExit(ClientData)
{
   Scheduler::instance()->quit();
   Scheduler::instance()->waitForShutdown();

   ACE_Reactor::end_event_loop(); 

   // give reactor some time to clean up
   sleep(1);
}

void start(const char *strategyNames)
{
   Scheduler::instance()->runStrategies(paraGlobal, strategyNames);
}

void stop()
{
   Scheduler::instance()->stop();
}

void wrapup()
{
   Scheduler::instance()->wrapUp();
}
