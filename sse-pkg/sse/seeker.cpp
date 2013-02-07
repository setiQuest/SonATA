/*******************************************************************************

 File:    seeker.cpp
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

/*

  This program acts as a master controller for scheduling and
  controlling observations.
*/

#include <ace/Reactor.h>
#include <ace/Synch.h>
#include <ace/Thread_Manager.h>
#include <ace/Synch.h>

#include "seeker.h"
#include "Site.h"
#include "Scheduler.h"
#include "NssParameters.h"
#include "ActivityWrappers.h"
#include "ActStratWrappers.h"
#include "SseArchive.h"
#include "ActParameters.h"
#include "sseVersion.h"
#include "SseException.h"
#include "Followup.h"
#include "MsgSender.h"
#include "NssAcceptHandler.h"
#include "SeekerCmdLineParser.h"

using namespace std;

static void blockAllSignals()
{
   sigset_t signal_set;
   if (sigfillset(&signal_set) == -1)
   {
      cerr << "failure calling sigfillset" << endl;
      return;
   }

   //  Put the <signal_set>.
   if (ACE_OS::pthread_sigmask (SIG_BLOCK, &signal_set, 0) != 0)
   {
      cerr << "failure calling pthread_sigmask SIG_BLOCK" << endl;
   }

}

static void addActStrategyTypesToScheduler(Scheduler * scheduler)
{
   scheduler->addActStrategyType(
      "obs", NewObsActStrategyWrapper);

   //JR - Add Tscope setup
   scheduler->addActStrategyType(
      "tscopesetup", NewTscopeSetupActStrategyWrapper);

   scheduler->addActStrategyType(
      "autoselectants", NewAutoselectAntsActStrategyWrapper);

   scheduler->addActStrategyType(
      "prepants", NewPrepAntsActStrategyWrapper);

   scheduler->addActStrategyType(
      "freeants", NewFreeAntsActStrategyWrapper);

   scheduler->addActStrategyType(
      "bfreset", NewBfResetActStrategyWrapper);

   scheduler->addActStrategyType(
      "bfinit", NewBfInitActStrategyWrapper);

   scheduler->addActStrategyType(
      "bfautoatten", NewBfAutoattenActStrategyWrapper);

   scheduler->addActStrategyType(
      "caldelay", NewCalDelayActStrategyWrapper);

   scheduler->addActStrategyType(
      "calphase", NewCalPhaseActStrategyWrapper);

   scheduler->addActStrategyType(
      "calfreq", NewCalFreqActStrategyWrapper);
}

static void addActivityTypesToScheduler(Scheduler * scheduler)
{
   Followup *followup = Followup::instance();

   // Activities are named so that they come out in the 
   // correct followup order when sorted.
   const string targetName("target");
   const string target1OnName("target1-on");
   const string target1OffName("target1off");
   const string target2OnName("target2-on");
   const string target2OffName("target2off");
   const string target3OnName("target3-on");
   const string target3OffName("target3off");
   const string target4OnName("target4-on");
   const string target4OffName("target4off");
   //const string target5OnNoFollowupName("target5-on-nofollowup");
   const string targetnOnName("targetn-on"); //targetn-on is forever
   const string targetnOffName("targetn-off");

   scheduler->addActivityType(targetName, ActivityEntry::UsesTargets,
                              NewTargetActWrapper);
   followup->setFollowupActType(targetName, target1OnName);

   scheduler->addActivityType(target1OnName, ActivityEntry::UsesTargets,
                              NewTargetOnActWrapper);
   followup->setFollowupActType(target1OnName, target1OffName);

   scheduler->addActivityType(target1OffName, ActivityEntry::UsesTargets,
                              NewTargetOffActWrapper);
   followup->setFollowupActType(target1OffName, target2OnName);

   scheduler->addActivityType(target2OnName, ActivityEntry::UsesTargets,
                              NewTargetOnActWrapper);
   followup->setFollowupActType(target2OnName, target2OffName);

   scheduler->addActivityType(target2OffName, ActivityEntry::UsesTargets,
                              NewTargetOffActWrapper);
   followup->setFollowupActType(target2OffName, target3OnName);

   scheduler->addActivityType(target3OnName, ActivityEntry::UsesTargets,
                              NewTargetOnActWrapper);
   followup->setFollowupActType(target3OnName, target3OffName);

   scheduler->addActivityType(target3OffName, ActivityEntry::UsesTargets,
                              NewTargetOffActWrapper);
   followup->setFollowupActType(target3OffName, target4OnName);

   scheduler->addActivityType(target4OnName, ActivityEntry::UsesTargets,
                              NewTargetOnActWrapper);
   followup->setFollowupActType(target4OnName, target4OffName);

   scheduler->addActivityType(target4OffName, ActivityEntry::UsesTargets,
                              NewTargetOffActWrapper);
   //
   //followup->setFollowupActType(target4OffName, target5OnNoFollowupName);
   //
   
   followup->setFollowupActType(target4OffName, targetnOnName);
   scheduler->addActivityType(targetnOnName, ActivityEntry::UsesTargets,
                              NewTargetOffActWrapper);
		   
   followup->setFollowupActType(targetnOnName, targetnOffName);
   scheduler->addActivityType(targetnOffName, ActivityEntry::UsesTargets,
                              NewTargetOffActWrapper);
	   
   followup->setFollowupActType(targetnOffName, targetnOnName);


   // type the last in the followup chain:

  // scheduler->addActivityType(target5OnNoFollowupName, ActivityEntry::UsesTargets,
  //                           NewTargetOnNoFollowupActWrapper);

   // Make this activity its own followup type, so that the followup lookup
   // table activity-chain can be validated (even though this type should
   // never invoke a followup).
  // followup->setFollowupActType(target5OnNoFollowupName,
   //                             target5OnNoFollowupName);


   const string gridWestName("gridwest");
   const string gridSouthName("gridsouth");
   const string gridOnName("gridon");
   const string gridNorthName("gridnorth");
   const string gridEastName("grideast");

   scheduler->addActivityType(gridWestName, ActivityEntry::UsesTargets,
                              NewGridWestStarActWrapper);
   followup->setFollowupActType(gridWestName, gridSouthName);

   scheduler->addActivityType(gridSouthName, ActivityEntry::UsesTargets,
                              NewGridSouthStarActWrapper);
   followup->setFollowupActType(gridSouthName, gridOnName);

   scheduler->addActivityType(gridOnName, ActivityEntry::UsesTargets,
                              NewGridOnStarActWrapper);
   followup->setFollowupActType(gridOnName, gridNorthName);

   scheduler->addActivityType(gridNorthName, ActivityEntry::UsesTargets,
                              NewGridNorthStarActWrapper);
   followup->setFollowupActType(gridNorthName, gridEastName);

   scheduler->addActivityType(gridEastName, ActivityEntry::UsesTargets,
                              NewGridEastStarActWrapper);
   followup->setFollowupActType(gridEastName, gridWestName);

   scheduler->addActivityType("tscopesetup", ActivityEntry::NoTargets,
                              NewTscopeSetupActWrapper);

   scheduler->addActivityType("cal", ActivityEntry::UsesTargets,
                              NewCalibrateActWrapper);

   scheduler->addActivityType("autoselectants", ActivityEntry::NoTargets,
                              NewAutoselectAntsActWrapper);

   scheduler->addActivityType("prepants", ActivityEntry::NoTargets,
                              NewPrepAntsActWrapper);

   scheduler->addActivityType("freeants", ActivityEntry::NoTargets,
                              NewFreeAntsActWrapper);

   scheduler->addActivityType("bfreset", ActivityEntry::NoTargets,
                              NewBeamformerResetActWrapper);

   scheduler->addActivityType("bfinit", ActivityEntry::NoTargets,
                              NewBeamformerInitActWrapper);

   scheduler->addActivityType("bfautoatten", ActivityEntry::UsesTargets,
                              NewBeamformerAutoattenActWrapper);

   scheduler->addActivityType("pointantswait", ActivityEntry::UsesTargets,
                              NewPointAntsAndWaitActWrapper);


   // Test (rf/if/dx) observations

   scheduler->addActivityType("birdiescan", ActivityEntry::NoTargets,
                              NewBirdieScanActWrapper);
   scheduler->addActivityType("rfiscan", ActivityEntry::NoTargets,
                              NewRfiScanActWrapper);
   scheduler->addActivityType("rfbirdiescan", ActivityEntry::NoTargets,
                              NewRfBirdieScanActWrapper);


   scheduler->addActivityType("rftest", ActivityEntry::NoTargets,
                              NewRfTestActWrapper);
   followup->setFollowupActType("rftest", "rftestfollowup");

   scheduler->addActivityType("rftestfollowup", ActivityEntry::NoTargets,
                              NewRfTestFollowupActWrapper);
   followup->setFollowupActType("rftestfollowup", "rftestfollowup");

   scheduler->addActivityType("rftestzerodrift", ActivityEntry::NoTargets,
                              NewRfTestZeroDriftActWrapper);

   scheduler->addActivityType("rftestforcedarchive", ActivityEntry::NoTargets,
                              NewRfTestForcedArchiveActWrapper);

   scheduler->addActivityType("iftest", ActivityEntry::NoTargets,
                              NewIfTestActWrapper);
   followup->setFollowupActType("iftest", "ifteston");

   scheduler->addActivityType("ifteston", ActivityEntry::NoTargets,
                              NewIfTestFollowUpOnActWrapper);
   followup->setFollowupActType("ifteston", "iftestoff");

   scheduler->addActivityType("iftestoff", ActivityEntry::NoTargets,
                              NewIfTestFollowUpOffActWrapper);
   followup->setFollowupActType("iftestoff", "ifteston");

   scheduler->addActivityType("iftestzerodrift", ActivityEntry::NoTargets,
                              NewIfTestZeroDriftActWrapper);

   scheduler->addActivityType("dxtest", ActivityEntry::NoTargets,
                              NewDxTestActWrapper);

   scheduler->addActivityType("datacollect", ActivityEntry::NoTargets,
                              NewDataCollectActWrapper);

   followup->validateFollowupActivitySequences();

}

static void *seekerThread(void *seekerThreadArgsVoidStar);

struct SeekerThreadArgs {
   SeekerCmdLineParser *seekerParser;
   ACE_Thread_Mutex *startingSeeker;
};

int seekerStart(int argc, char * argv[], ACE_Thread_Mutex &startingSeeker)
{
   SeekerCmdLineParser *seekerParser = new SeekerCmdLineParser();
   if (! seekerParser->parse(argc, argv))
   {
      cerr << seekerParser->getErrorText() << endl;
      cerr << seekerParser->getUsage();
      exit(1);
   }
   SeekerThreadArgs *seekerThreadArgs = new SeekerThreadArgs;
   seekerThreadArgs->seekerParser = seekerParser;
   seekerThreadArgs->startingSeeker = &startingSeeker;

   //  Tcl_EvalFile(interp, arglist->script_file);

   // block all signals so that:
   // 1. ctrl-c is trapped to prevent accidental program exit
   // 2. signals don't cause the ACE Reactor to fail and exit
   //    prematurely

   blockAllSignals();

   // Spawn the seeker thread using ACE.
   ACE_Thread_Manager::instance()->
      spawn(static_cast<ACE_THR_FUNC>(seekerThread),
	    seekerThreadArgs);

   return 0;
}

static void *seekerThread(void *seekerThreadArgsVoidStar)
{
   SeekerThreadArgs *seekerThreadArgs =
      static_cast<SeekerThreadArgs *>(seekerThreadArgsVoidStar);
   SeekerCmdLineParser *seekerParser = seekerThreadArgs->seekerParser;
   ACE_Thread_Mutex *startingSeeker = seekerThreadArgs->startingSeeker;
   delete seekerThreadArgs;

   SseArchive::setup();
   SseArchive::SystemLog() << "Seeker started.  " 
                           << SSE_VERSION << endl;

   Scheduler *scheduler = Scheduler::instance();
   Site* site;
   try {
      site = new Site(seekerParser->getDxPort(),
                      seekerParser->getZxPort(),
                      seekerParser->getDxArchiverPort(),
                      seekerParser->getChannelizerPort(),
                      seekerParser->getDxArchiver1Hostname(),
                      seekerParser->getDxToArchiver1Port(),
                      seekerParser->getDxArchiver2Hostname(),
                      seekerParser->getDxToArchiver2Port(),
                      seekerParser->getDxArchiver3Hostname(),
                      seekerParser->getDxToArchiver3Port(),
                      seekerParser->getIfcPort(),
                      seekerParser->getTscopePort(),
                      seekerParser->getTsigPort(),
                      seekerParser->getComponentControlPort(),
                      seekerParser->getExpectedComponentsFilename(),
                      seekerParser->getNoUi());
      delete seekerParser;
    
      paraGlobal.setSite(site);  // attach the site to the Text UI
      paraGlobal.act_->setScheduler(scheduler);
      scheduler->setSite(site);

      addActivityTypesToScheduler(scheduler);
      addActStrategyTypesToScheduler(scheduler);
   }
   catch (NssAcceptHandlerException &exception) {
      scheduler->failed();

      stringstream strm;
      strm << "Scheduler failed to start. " << exception.descrip(); 
      SseMessage::log(MsgSender,
                      NSS_NO_ACTIVITY_ID, SSE_MSG_SCHED_FAILED,
                      SEVERITY_ERROR, strm.str(),
                      __FILE__, __LINE__);

      cerr << exception.descrip() 
           << "Warning: failed to start one or more component accept" 
           << " handlers." << endl
           << "Make sure that another copy of this program"
           << " is not already running" << endl
           << "on this machine." << endl
           << "If new component port numbers were specified, "
           << "make sure they're valid."
           << endl;
    
      exit(1);

   }
   catch (SseException &exception) {
      scheduler->failed();

      stringstream strm;
      strm << "Scheduler failed to start. " << exception.descrip(); 
      SseMessage::log(MsgSender,
                      NSS_NO_ACTIVITY_ID, SSE_MSG_SCHED_FAILED,
                      SEVERITY_ERROR, strm.str(),
                      __FILE__, __LINE__);

      cerr << strm.str();

      exit(1);

   }
   catch (...) {
      cerr << "seeker main: caught unknown exception" << endl;
      SseMessage::log(MsgSender,
                      NSS_NO_ACTIVITY_ID, SSE_MSG_EXCEPTION,
                      SEVERITY_ERROR, 
                      "seeker main: caught unknown exception",
                      __FILE__, __LINE__);

      exit(1);
   }

   startingSeeker->release();

   // Prevent the ACE Reactor event loop from exiting prematurely
   // when a system call is interrupted (ie, errno == EINTR)
   // by enabling restart.
   //
   ACE_Reactor::instance()->restart(1);

   //int status =
   //
   ACE_Reactor::run_event_loop();

   SseArchive::SystemLog() << "Seeker done" << endl;

   return 0;
}
