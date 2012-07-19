/*******************************************************************************

 File:    ActivityStrategy.cpp
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


#include "ActivityStrategy.h"
#include "Activity.h"
#include "ActivityException.h"
#include "ActParameters.h"
#include "Assert.h"
#include "AtaInformation.h"
#include "ComponentControlList.h"
#include "ComponentControlProxy.h"
#include "DebugLog.h"
#include "ExpectedNssComponentsTree.h"
#include "IdNumberFactory.h"
#include "MessageBlock.h"
#include "MsgSender.h"
#include "MysqlQuery.h"
#include "NssComponentTree.h"
#include "NssParameters.h"
#include "Scheduler.h"
#include "SchedulerParameters.h"
#include "Site.h"
#include "SiteView.h"
#include "SseArchive.h"
#include "SseAstro.h"
#include "SseMessage.h"
#include "SseUtil.h"
#include "TargetPosition.h"
#include "TscopeParameters.h"
#include "TscopeProxy.h"
#include "UnitOfWork.h"
#include <algorithm>
#include <sstream>

using namespace std;

static const char * ActivityIdFilename = "ActivityId.txt";

typedef Message_Block<Unit_Of_Work<ActivityStrategy> > MsgBlock;

// Put the "unit of work" into the message queue.
// Do this as a macro so that the MsgBlock typedef can be used by 
// the caller.

#define PutMsgInQueue(message) \
  if (this->putq((message)) == -1) \
  { \
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, \
		SSE_MSG_ACT_STRAT_FAILED, \
		SEVERITY_ERROR, \
		"ActivityStrategy putq error\n", \
		__FILE__, __LINE__); \
  } 


class ActivityStartTerminatedDueToWrapupException : public SseException
{
public:
   ActivityStartTerminatedDueToWrapupException(const string& what)
      : SseException(what)
   {
   }
};

class TscopeNotReadyException : public SseException
{
public:
   TscopeNotReadyException(const string& what)
      : SseException(what)
   {
   }
};

class WorkStrategySetVerbose : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkStrategySetVerbose(int message,
			  int verboseLevel)
      : Unit_Of_Work<ActivityStrategy>(message),
      verboseLevel_(verboseLevel)
   {
   }
   virtual int call(ActivityStrategy* strategy);
protected:
   int verboseLevel_;
};

class WorkStart : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkStart(int message)
      : Unit_Of_Work<ActivityStrategy>(message)
   {
   }
   virtual int call(ActivityStrategy* strategy);
};

class WorkStrategyStop : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkStrategyStop(int message)
      : Unit_Of_Work<ActivityStrategy>(message)
   {
   }
   virtual int call(ActivityStrategy* strategy);
};

class WorkStrategyWrapUp : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkStrategyWrapUp(int message)
      : Unit_Of_Work<ActivityStrategy>(message)
   {
   }
   virtual int call(ActivityStrategy* strategy);
};


class WorkDataCollectionComplete : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkDataCollectionComplete(int message, Activity *activity)
      : Unit_Of_Work<ActivityStrategy>(message),
      activity_(activity)
   {
   }
   virtual int call(ActivityStrategy* strategy);
protected:
   Activity *activity_;
};

class WorkFoundConfirmedCandidates : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkFoundConfirmedCandidates(int message, Activity *activity)
      : Unit_Of_Work<ActivityStrategy>(message),
      activity_(activity)
   {
   }
   virtual int call(ActivityStrategy* strategy);
protected:
   Activity *activity_;
};


class WorkAttemptToStartNextActivity : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkAttemptToStartNextActivity(int message)
      : Unit_Of_Work<ActivityStrategy>(message)
   {
   }
   virtual int call(ActivityStrategy* strategy);
};

class WorkContinueWithAnyMoreActivities : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkContinueWithAnyMoreActivities(int message)
      : Unit_Of_Work<ActivityStrategy>(message)
   {
   }
   virtual int call(ActivityStrategy* strategy);
};


class WorkActivityComplete : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkActivityComplete(int message, Activity *activity, bool failed)
      : Unit_Of_Work<ActivityStrategy>(message),
      activity_(activity),
      failed_(failed)
   {
   }
   virtual int call(ActivityStrategy* strategy);
protected:
   Activity *activity_;
   bool failed_;
};

class WorkShutdown : public Unit_Of_Work<ActivityStrategy>
{
public:
   WorkShutdown(int message)
      : Unit_Of_Work<ActivityStrategy>(message)
   {
   }
   virtual int call(ActivityStrategy* strategy);
   virtual bool quit() const
   {
      return true;
   }
};

ActivityStrategy::ActivityStrategy(Scheduler *scheduler,
				   Site *site, 
				   const NssParameters &nssParameters,
                                   int verboseLevel):
   numberActivitiesStarted_(0),
   strategyComplete_(false),
   schedulerReleased_(false),
   nssParameters_(nssParameters),
   verboseLevel_(verboseLevel),
   stop_(false),
   wrapUp_(false),
   messageCount_(0),
   numberThreads_(1),
   barrier_(numberThreads_),
   scheduler_(scheduler),
   site_(site),
   strategyRepeatCount_(nssParameters_.sched_->getRepeatCount()),
   sequentialFailedActivitiesCount_(0),
   maxSequentialFailedActivities_(nssParameters_.sched_->getMaxActFailures()),
   restartPauseSecs_(nssParameters_.sched_->getPauseTimeBetweenActRestartsSecs()),
   failedStartCount_(0),
   waitingForTscopeReady_(false),
   tscopeReadyWaitIntervalSecs_(nssParameters_.sched_->getTscopeReadyPauseSecs()), 
   tscopeReadyMaxFailures_(nssParameters_.sched_->getTscopeReadyMaxFailures()),
   restartTimeout_("restart"),
   finalCleanupAlreadyRun_(false),
   siteView_(0)
{
   VERBOSE2(getVerboseLevel(), "ActivityStrategy::ActivityStrategy" << endl;);

   siteView_ = new SiteView(
      nssParameters_.tscope_->getSiteLongWestDeg(),
      nssParameters_.tscope_->getSiteLatNorthDeg(),
      nssParameters_.tscope_->getSiteHorizDeg());
}
  
int ActivityStrategy::getVerboseLevel() const
{
   return verboseLevel_.value();
}

int ActivityStrategy::setVerboseLevel(int verboseLevel)
{
   MsgBlock *message = new MsgBlock(new WorkStrategySetVerbose(
      messageCount_++, verboseLevel));

   PutMsgInQueue(message);

   return verboseLevel;
}

int ActivityStrategy::setVerboseLevelInternal(int verboseLevel)
{
   verboseLevel_ = verboseLevel;

   setVerboseLevelInternalHook(verboseLevel);

   return verboseLevel_.value();
}

void ActivityStrategy::setVerboseLevelInternalHook(int verboseLevel)
{
   // allow subclasses to override
}

void ActivityStrategy::sendWorkStartMsg()
{
   MsgBlock *message = new MsgBlock(new WorkStart(messageCount_++));
   PutMsgInQueue(message);
}


void ActivityStrategy::sendWorkAttemptToStartNextActivityMsg()
{
   MsgBlock *message = new MsgBlock(new WorkAttemptToStartNextActivity(messageCount_++));
   PutMsgInQueue(message);
}

void ActivityStrategy::sendWorkContinueWithAnyMoreActivitiesMsg()
{
   MsgBlock *message = new MsgBlock(new WorkContinueWithAnyMoreActivities(messageCount_++));
   PutMsgInQueue(message);
}

int ActivityStrategy::start()
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::start()" 
	    << endl;);

   int result = this->activate(THR_NEW_LWP, numberThreads_);

   if (result == -1)
   {
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID,
                      SSE_MSG_ACT_STRAT_FAILED,
                      SEVERITY_ERROR,
                      "start() task activate failed",
                      __FILE__, __LINE__);

      return result;
   }
   else if (result == 1)
   {
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, 
                      SSE_MSG_ACT_STRAT_FAILED,
                      SEVERITY_ERROR,
                      "start() task is already an active object",
                      __FILE__, __LINE__);

      return result;
      
   }

   sendWorkStartMsg();

   return result;
}

void ActivityStrategy::stop()
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::stop()"  << endl;);

   MsgBlock *message = new MsgBlock(new WorkStrategyStop(messageCount_++));
   PutMsgInQueue(message);

}

void ActivityStrategy::wrapUp()
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::wrapUp()"
	    << endl;);

   stringstream strm;
   strm << "Wrapping up strategy, waiting for activities "
        << "and any followups to complete..." << endl;

   SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INFO,
                   SEVERITY_INFO, strm.str());
   cerr << strm.str();
  
   MsgBlock *message = new MsgBlock(new WorkStrategyWrapUp(
      messageCount_++));

   PutMsgInQueue(message);

}


void ActivityStrategy::shutdown()
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::shutdown()"  << endl;);

   MsgBlock *message = new MsgBlock(new WorkShutdown(messageCount_++));

   PutMsgInQueue(message);

}

void ActivityStrategy::shutdownInternal()
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::shutdownInternal()"  << endl;);

}


void ActivityStrategy::dataCollectionComplete(Activity *activity)
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::dataCollectionComplete() " 
	    << "Act: " << activity->getId() << endl;);

   MsgBlock *message = new MsgBlock(new WorkDataCollectionComplete(
      messageCount_++, activity));

   PutMsgInQueue(message);

}

void ActivityStrategy::foundConfirmedCandidates(Activity *activity)
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::foundConfirmedCandidates() "
	    << "Act: " << activity->getId() << endl;);

   MsgBlock *message = new MsgBlock(new WorkFoundConfirmedCandidates(
      messageCount_++, activity));

   PutMsgInQueue(message);

}

void ActivityStrategy::activityComplete(Activity *activity, bool failed)
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::activityComplete() "
	    << "Act: " << activity->getId() << endl;);

   // send WorkActivityComplete (clean up the activity)
   MsgBlock *message = new MsgBlock(new WorkActivityComplete(
      messageCount_++, activity, failed));
    
   PutMsgInQueue(message);

}


bool ActivityStrategy::isStrategyComplete() const
{  
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(stratComplMutex_);

   return strategyComplete_;
}

void ActivityStrategy::setStrategyComplete(bool complete)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(stratComplMutex_);

   strategyComplete_ = complete;
}



bool ActivityStrategy::isStopRequested() const
{
   return stop_.value();
}

bool ActivityStrategy::isWrapUpRequested() const
{
   return wrapUp_.value();
}

bool ActivityStrategy::isRestartPauseActive() const
{
   return restartTimeout_.isTimerActive();
}

/*
  Stop all running activities.  To avoid potential
  deadlock that could be caused by the activity calling
  back into this class as a result of the stop command,
  a copy of the activities list is grabbed first.
  I.e. minimize use of the activitiesMutex_.
*/
void ActivityStrategy::stopAllActivities()
{
   vector<Activity *> localActList;
   {
      ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activitiesMutex_);
      
      for (ActivityList::iterator index = activities_.begin();
	   index != activities_.end(); ++index)
      {
	 localActList.push_back(*index);
      }
   }

   for (unsigned int i=0; i<localActList.size(); ++i)
   {
      localActList[i]->stop();
   }

}

void ActivityStrategy::stopInternal()
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::stopInternal()"  << endl;);

   stop_ = true;
   restartTimeout_.cancelTimer();

   // Give any running activities a chance to stop & wrap up.
   if (numberOfRunningActivities() > 0)
   {
      stopAllActivities();
   }
   else 
   {
      // No acts in progress, cleanup the strategy immediately.
      strategyFailed();
   }
}

void ActivityStrategy::wrapUpInternal()
{
   VERBOSE2(verboseLevel_, "ActivityStrategy::wrapUpInternal()"  << endl;);

   wrapUp_ = true;
}



//  Like the thread-pools before, this is where all of the work is done.
int ActivityStrategy::svc(void)
{
   // Wait for all threads to get this far before continuing.
   this->barrier_.wait();

  // ACE_DEBUG((LM_DEBUG, "(%P|%t) Task 0x%x starts in thread %u\n", (void *) this, ACE_Thread::self()));

  // getq() wants an ACE_Message_Block so we'll start out with one
  // of those.  We could do some casting (or even auto-casting) to
  // avoid the extra variable but I prefer to be clear about our actions.
   ACE_Message_Block *message;

   // What we really put into the queue was our Message_Block.
   // After we get the message from the queue, we'll cast it to this
   // so that we know how to work on it.
   MsgBlock *message_block;

   // And, of course, our Message_Block_Strategy contains our Data_Block
   // instead of the typical ACE_Data_Block
   Data_Block<Unit_Of_Work<ActivityStrategy> > *data_block;

  // Even though we put Work objects into the queue, we take them
  // out using the baseclass pointer.  This allows us to create new
  // derivatives without having to change this svc() method.
   Unit_Of_Work<ActivityStrategy> *work;

   while (! isStrategyComplete())
   {
      // Get the ACE_Message_Block
      if (this->getq(message) == -1)
      {
	 SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_ACT_STRAT_FAILED,
                         SEVERITY_ERROR,
                         "svc() getq failed",
                         __FILE__, __LINE__);
	 return -1;
      }
      
      // "Convert" it to our Message_Block_Strategy
      message_block = dynamic_cast<MsgBlock *>(message);
      
      // Get the ACE_Data_Block and "convert" to Data_Block in one step.
      data_block = dynamic_cast<Data_Block<Unit_Of_Work<ActivityStrategy> > *>(message_block->data_block());
      
      // Get the unit of work from the data block
      work = data_block->data();
      
      // If this isn't a hangup/shutdown message then we tell the
      // unit of work to process() for a while.
      
      try {
	 work->call(this);
      }
      catch (SseException &except)
      {
	 SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, except.code(),
                         except.severity(), except.descrip(), 
                         except.sourceFilename(), except.lineNumber());

         stop();
      }

      if (work->quit())
      {
	 break;
      }
      
      // If we don't have subtasks then invoke fini() to tell
      // the unit of work that we won't be invoking process()
      // any more.  Then release() the block.  This release()
      // would not change if we duplicate()ed in the above conditional

      work->fini();
      message_block->release();

   }

   return 0;
}


int WorkStrategySetVerbose::call(ActivityStrategy* strategy)
{
   strategy->setVerboseLevelInternal(verboseLevel_);
   return 0;
}

int WorkStart::call(ActivityStrategy* strategy)
{
   strategy->startInternal();
   return 0;
}

int WorkShutdown::call(ActivityStrategy* strategy)
{
   strategy->shutdownInternal();
   return 0;
}

int WorkStrategyStop::call(ActivityStrategy* strategy)
{
   strategy->stopInternal();
   return 0;
}

int WorkStrategyWrapUp::call(ActivityStrategy* strategy)
{
   strategy->wrapUpInternal();
   return 0;
}

int WorkDataCollectionComplete::call(ActivityStrategy* strategy)
{
   strategy->dataCollectionCompleteInternal(activity_);
   return 0;
}

int WorkFoundConfirmedCandidates::call(ActivityStrategy* strategy)
{
   strategy->foundConfirmedCandidatesInternal(activity_);
   return 0;
}

int WorkAttemptToStartNextActivity::call(ActivityStrategy* strategy)
{
   strategy->attemptToStartNextActivity();
   return 0;
}

int WorkContinueWithAnyMoreActivities::call(ActivityStrategy* strategy)
{
   strategy->continueWithAnyMoreActivities();
   return 0;
}

int WorkActivityComplete::call(ActivityStrategy* strategy)
{
   strategy->activityCompleteInternal(activity_, failed_);
   return 0;
}

ActivityStrategy::~ActivityStrategy()
{
   VERBOSE2(getVerboseLevel(), "ActivityStrategy::~ActivityStrategy\n" );
   delete siteView_;
}



void ActivityStrategy::getRunningActivityIds(
   ActivityIdList & activityIdList)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activitiesMutex_);
   
   for (ActivityList::const_iterator index = activities_.begin();
	index != activities_.end(); ++index)
   {
      ActivityId_t activityId = (*index)->getId();
      activityIdList.push_back(activityId);
   }
}



/* Checks the validity of all targets specified in the nssParameters:
   
   1. That all targets are available (visible, and with enough time left)
   2. (TBD) that all synthesized beam targets fall within the primary beam
         (need tuning freq for this).

*/
void ActivityStrategy::validateChosenTargets(
   const NssParameters & nssParams,
   bool *allTargetsAvailable, bool *allTargetsInPrimaryFov)
{
   VERBOSE2(getVerboseLevel(), "ActivityStrategy::validateChosenTargets()" << endl;);

   *allTargetsAvailable = true;
   *allTargetsInPrimaryFov = true;

   if (nssParams.sched_->getCheckTargetsOption() == 
       SchedulerParameters::CHECK_TARGETS_OFF)
   {
      return;
   }

   // only check the targets for those activity types that use them
   if (scheduler_->getActivityTypeTargetUse(nssParams.act_->getActivityType()) 
       == ActivityEntry::NoTargets)
   {
      return;
   }

   vector<string> allBeamsToUse(nssParams.sched_->getBeamsToUse());
   vector<TargetId> targetIds;

   stringstream beamStrm;
   for (vector<string>::iterator it = allBeamsToUse.begin();
	it != allBeamsToUse.end(); ++it)
   {
      string & beamName = *it;
      TargetId targetId(nssParams.act_->getTargetIdForBeam(beamName));

      beamStrm << beamName << ": " << targetId << "  ";

      targetIds.push_back(targetId);
   }

   if (nssParameters_.act_->getPrimaryBeamPositionType() == 
       ActParameters::PRIMARY_BEAM_POS_TARGET_ID)
   {
      TargetId primaryTargetId(nssParams.act_->getPrimaryTargetId());
      beamStrm << "primary: " << primaryTargetId << " ";
      targetIds.push_back(primaryTargetId);
   }
   else 
   {
      // TBD verify that primary beam coords are visible
   }

   if (! areAllTargetsAvailable(targetIds))
   {
      *allTargetsAvailable = false;
   }

   if (! *allTargetsAvailable || ! *allTargetsInPrimaryFov)
   {
      stringstream msg;

      msg << "ActivityStrategy::validateChosenTargets(): " 
	  << " Trying to start activity type: " 
	  << nssParams.act_->getActivityType() << ".  ";

      if (! *allTargetsAvailable)
      {
	   msg  << "One or more targets are not available. ";
      }  

      if (! *allTargetsInPrimaryFov)
      {
	 msg << "One or more targets are outside the primary beam FOV. ";
      }  
      msg << endl;

      msg << "Beam assignments: " << beamStrm.str() << endl;

      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, 
                      SSE_MSG_INVALID_TARGET,
                      SEVERITY_INFO, msg.str(),
                      __FILE__, __LINE__);
   }

   if (nssParams.sched_->getCheckTargetsOption() == 
       SchedulerParameters::CHECK_TARGETS_WARN)
   {
      *allTargetsAvailable = true;
      *allTargetsInPrimaryFov = true;
   }


}

/*
  Check the availability of all the targets in the targetIds list.
  A target is available if it's visible and there's enough time
  left to do a full data collection.

  If all targets are available, return true.
  Otherwise log those that are not available, and why.

  Throws SseException if any targetIds are not found in the target catalog.
 */
bool ActivityStrategy::areAllTargetsAvailable(vector<TargetId> targetIds)
{
   if (!nssParameters_.db_->useDb())
   {
      throw SseException(
	 "Database must be turned on to check for available targets\n",
	 __FILE__, __LINE__, SSE_MSG_DBERR, SEVERITY_ERROR);
   }

   // Look up the coordinates of each target
   stringstream targetCatQuery;
   targetCatQuery << "select targetId, ra2000Hours, dec2000Deg, "
                  << "pmRaMasYr, pmDecMasYr, parallaxMas "
		  << "from TargetCat WHERE ";
   
   for (unsigned int i=0; i < targetIds.size(); ++i)
   {
      targetCatQuery << " targetId = " << targetIds[i] << " or ";
   }
   targetCatQuery << " false ";  // take care of the last 'or' clause

   enum resultCols { targetIdCol, ra2000HoursCol, dec2000DegCol,
		     pmRaCol, pmDecCol, parallaxCol, numCols };

   MYSQL * db(nssParameters_.db_->getDb());
   MysqlQuery query(db);
   query.execute(targetCatQuery.str(), numCols, __FILE__, __LINE__);

   time_t currentTime;
   time(&currentTime);

   Assert(siteView_);
   double lmstRads = siteView_->lmstRads(currentTime);

   bool allAvailable(true);
   vector<TargetId> targetsNotVisible;
   vector<TargetId> targetsTooCloseToSetting;
   vector<TargetId> targetIdsNotFound(targetIds);

   // check availability for all targets
   while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
   {
      TargetId targetId(query.getInt(row, targetIdCol, 
				     __FILE__, __LINE__));

      double ra2000Rads(SseAstro::hoursToRadians(
	 query.getDouble(row, ra2000HoursCol, __FILE__, __LINE__)));
      
      double dec2000Rads(SseAstro::degreesToRadians(
	 query.getDouble(row, dec2000DegCol, __FILE__, __LINE__)));
      
      double pmRaMasYr(query.getDouble(row, pmRaCol, __FILE__, __LINE__));
      
      double pmDecMasYr(query.getDouble(row, pmDecCol, __FILE__, __LINE__));
      
      double parallaxMas(query.getDouble(row, parallaxCol,
                                         __FILE__, __LINE__));

      time_t riseTime(0);
      time_t setTime(0);
      bool isVisible = isTargetVisible(
	 ra2000Rads, dec2000Rads, pmRaMasYr, pmDecMasYr, parallaxMas,
	 currentTime, lmstRads, &riseTime, &setTime);
      
      if (isVisible)
      {
	 logRiseSetTimes(targetId, riseTime, setTime);

	 if (isTargetTooCloseToSetting(currentTime, setTime))
	 {
	    targetsTooCloseToSetting.push_back(targetId);
	    allAvailable = false;
	 }
      }
      else 
      {
	 targetsNotVisible.push_back(targetId);
	 allAvailable = false;
      }

      // keep track of target ids processed so far
      targetIdsNotFound.erase(remove(targetIdsNotFound.begin(),
				     targetIdsNotFound.end(), targetId),
			      targetIdsNotFound.end());

   }

   if (targetIdsNotFound.size() > 0)
   {
      throwTargetIdsNotFoundException(targetIdsNotFound);
   }

   if (! allAvailable)
   {
      logUnavailableTargets(targetsNotVisible, targetsTooCloseToSetting);
   }

   return allAvailable;
}

 
void ActivityStrategy::logRiseSetTimes(
   TargetId targetId, time_t riseTime, time_t setTime)
{
   SseArchive::SystemLog() 
      << "Target: " << targetId 
      << "  rise: " << SseUtil::isoDateTime(riseTime)
      << "  set: " << SseUtil::isoDateTime(setTime) << endl;
}

/*
  See if there's enough time left on this target
*/
bool ActivityStrategy::isTargetTooCloseToSetting(
   time_t currentTime, time_t setTime)
{
   // let subclasses override this method as desired

   return false;
}

/*
  Determine target visibility at the specified lmst & current time
  for the J2000 target coords.
  If target is visible, then the riseTime and setTime
  args will contain valid rise/set information and the function
  will return true;
 */
bool ActivityStrategy::isTargetVisible(
   double ra2000Rads, double dec2000Rads, double pmRaMasYr, double pmDecMasYr,
   double parallaxMas, time_t currentTime, double lmstRads,
   time_t *riseTime, time_t *setTime)
{
   // get position for the current time
   RaDec targetRaDec = RaDec(Radian(ra2000Rads), Radian(dec2000Rads));
   TargetPosition targetPos(targetRaDec, pmRaMasYr, pmDecMasYr, parallaxMas);

   RaDec targetRaDecNow = targetPos.positionAtNewEpochAndEquinox(currentTime);
   
   // check visibility
   double timeSinceRiseRads(-1);
   double timeUntilSetRads(-1);
   bool isVisible = siteView_->isTargetVisible(
      lmstRads, targetRaDecNow, timeSinceRiseRads, timeUntilSetRads);

   if (isVisible)
   {
      *riseTime = currentTime - 
	 SseAstro::siderealRadsToSolarTimeSecs(timeSinceRiseRads);
      
      *setTime = currentTime + 
	 SseAstro::siderealRadsToSolarTimeSecs(timeUntilSetRads);
   }

   return isVisible;
}

void ActivityStrategy::logUnavailableTargets(
   const vector<TargetId> & targetsNotVisible, 
   const vector<TargetId> & targetsTooCloseToSetting)
{
   stringstream strm;
   strm << "Targets were requested that are not available.  ";
   
   printTargetIdsToStream(strm, "Targets not visible", 
			  targetsNotVisible);  
   
   printTargetIdsToStream(strm, "Targets too close to setting",
			  targetsTooCloseToSetting);
   strm << endl;
   
   SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, 
                   SSE_MSG_INVALID_TARGET,
                   SEVERITY_INFO, strm.str(),
                   __FILE__, __LINE__);
}

void ActivityStrategy::printTargetIdsToStream(
   ostream &strm, const string & title, const vector<TargetId> & targetIds)
{
   strm << title << ": ";
   
   if (targetIds.size() == 0)
   {
      strm << "None";
   }
   else 
   {
      for (unsigned int i=0; i < targetIds.size(); ++i)
      {
	 strm << " " << targetIds[i];
      }
   }
   strm << ".  ";
}



void ActivityStrategy::throwTargetIdsNotFoundException(
   const vector<TargetId> & targetIdsNotFound)
{
   stringstream strm;
   
   strm << "Error: These target Ids were not found "
	<< "in the database target catalog: ";
   
   for (vector<TargetId>::const_iterator it = targetIdsNotFound.begin();
	it != targetIdsNotFound.end(); ++it)
   {
      const TargetId & targetId(*it); 
      
      strm << targetId << " ";
   }
   strm << endl;
   
   throw SseException(strm.str(), __FILE__, __LINE__, 
		      SSE_MSG_DBERR, SEVERITY_ERROR);
}

void ActivityStrategy::strategyFailed()
{
   bool hasFailed(true);
   strategyCleanup(hasFailed);
}

void ActivityStrategy::strategySuccessful()
{
   bool hasFailed(false);
   strategyCleanup(hasFailed);
}

void ActivityStrategy::strategyCleanup(bool hasFailed)
{
   VERBOSE2(getVerboseLevel(), "ActivityStrategy::strategyCleanup()" << endl;);

   if (!schedulerReleased_)
   {
      scheduler_->releaseStrategyActive();

      schedulerReleased_ = true;
   }

   VERBOSE2(getVerboseLevel(), "ActivityStrategy::strategyCleanup() "
	    << "numberOfRunningActivities() = " 
	    << numberOfRunningActivities() << endl;);

   if ((numberOfRunningActivities() == 0) || isStopRequested())
   {
      if (!finalCleanupAlreadyRun_)
      {
	 finalCleanupAlreadyRun_ = true;

         strategyCleanupHook();

	 // cancel any pending restart attempts
	 restartTimeout_.cancelTimer();
	 
	 const string msg("Scheduler strategy complete");
	 cerr << msg << endl;
	 SseArchive::SystemLog() << msg << endl;
	 
	 setStrategyComplete(true);
	 scheduler_->strategyComplete(this, hasFailed);
      }
   }
}

void ActivityStrategy::strategyCleanupHook()
{
   // let subclasses override as needed
}

void ActivityStrategy::startInternal()
{
   VERBOSE2(getVerboseLevel(), "ActivityStrategy::startInternal()" << endl;);

   if (!nssParameters_.isValid()){
      stringstream strm;
      strm << "Parameters are not valid " << nssParameters_ << endl;
      strm << "Strategy failed to start: invalid parameter(s)" << endl;

      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID,
                      SSE_MSG_INVALID_PARMS, SEVERITY_ERROR,
                      strm.str(), __FILE__, __LINE__);

      strategyFailed();
      return;
   }

   VERBOSE2(getVerboseLevel(), "ActivityStrategy::startInternal() acquiring strategyActive" << endl;);
   scheduler_->acquireStrategyActive();

   startInternalHook();

   attemptToStartNextActivity();
}

void ActivityStrategy::startInternalHook()
{
   // let subclasses override as needed
}


void ActivityStrategy::dataCollectionCompleteInternal(Activity *activity)
{
   VERBOSE2(getVerboseLevel(), 
	    "ActivityStrategy::dataCollectionCompleteInternal()" << endl;);

   dataCollectionCompleteInternalHook(activity);

   continueWithAnyMoreActivities();
}

void ActivityStrategy::dataCollectionCompleteInternalHook(Activity *activity)
{
   // let subclasses override as needed
}


void ActivityStrategy::eraseActivity(Activity *activity)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activitiesMutex_);

   ActivityList::iterator index = 
      find(activities_.begin(), activities_.end(), activity);
   if (index == activities_.end())
   {
      throw SseException("Activity not found in eraseActivity\n",
                         __FILE__, __LINE__, 
                         SSE_MSG_EXCEPTION, SEVERITY_ERROR);
   }
   else
   {
      activities_.erase(index);
   }

}

void ActivityStrategy::cleanUpActivity(Activity *activity)
{ 
   cleanUpActivityHook(activity);
   eraseActivity(activity);
   
   /*
     Have the activity delete itself to avoid 
     any race conditions while it's cleaning up.
     Also assume that activity deletes NssComponentTree.
   */

   activity->destroy();
}

void ActivityStrategy::cleanUpActivityHook(Activity *activity)
{
   // let subclasses override as needed
}


void ActivityStrategy::activityCompleteInternal(
   Activity *activity, 
   bool failed)
{
   VERBOSE2(getVerboseLevel(),
	    "ActivityStrategy::activityCompleteInternal()" << endl;);

   cerr << "Activity " << activity->getId() << " is complete" << endl;

   try 
   {
      activityCompleteInternalHook(activity, failed);
      cleanUpActivity(activity);
      
      ActivityId_t actId(activity->getId());      
      if (failed)
      {
	 cerr << "ActivityStrategy: activityComplete: activity " 
	      << actId << " failed" << endl;
	 
	 sequentialFailedActivitiesCount_++;
	 
	 bool maxErrorsExceeded = sequentialFailedActivitiesCount_ >=
	    maxSequentialFailedActivities_;
	 
	 if (maxErrorsExceeded)
	 {
	    const string msg("Scheduler strategy: exceeded max allowed activity failures");
	    SseArchive::SystemLog() << msg << endl;
	    SseArchive::ErrorLog() << msg << endl;
	    cerr << msg << endl;

	    sendErrorEmail(msg);

	    stop();

	    return;
	 }
	 else 
	 {
	    // TBD recovery efforts to fix reason(s) for activity failure.

	 }

	 if (isRestartPauseActive())
	 {
	    return;
	 }

	 if (restartPauseSecs_ > 0 && ! isStopRequested())
	 {
            stringstream strm;
            strm << "ActivityStrategy: act failure, pausing "
                 << restartPauseSecs_ << " secs before re-attempting "
                 << "activity start...";

	    VERBOSE2(getVerboseLevel(), strm.str() << endl;);
	    cerr << strm.str() << endl;
	    SseArchive::SystemLog() << strm.str() << endl;

	    restartTimeout_.startTimer(
	       restartPauseSecs_,
	       this, &ActivityStrategy::sendWorkContinueWithAnyMoreActivitiesMsg,
	       getVerboseLevel());

	    return;
	 }
      }
      else  // activity succeeded
      {
	 sequentialFailedActivitiesCount_ = 0;
	 
	 cerr << "ActivityStrategy: activityComplete: activity "
	      << actId << " succeeded "<< endl;
      }

      sendWorkContinueWithAnyMoreActivitiesMsg();
   
   }
   catch (SseException & except)
   {
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, except.code(),
                      except.severity(), except.descrip(), 
                      except.sourceFilename(), except.lineNumber());

      VERBOSE2(getVerboseLevel(), "ActivityStrategy::activityCompleteInternal() caught exception, calling cleanup" << endl;);

      strategyFailed();
   }

}

void ActivityStrategy::activityCompleteInternalHook(Activity *activity, bool failed)
{
   // let subclasses override as desired
}

void ActivityStrategy::foundConfirmedCandidatesInternal(Activity *activity)
{
   // let subclasses override as needed
   // TBD: this probably shouldn't be in the base class interface,
   // but obs strat subclass needs it.  Generalize in some way or redesign.
}

string ActivityStrategy::getActivityStatus() const
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activitiesMutex_);

   if (isStrategyComplete())
   {
      return "SchedulerStrategy completed\n";
   }

   if (activities_.size() == 0)
   {
      return "SchedulerStrategy initialized (no activities running yet)\n";
   }

   stringstream strm;
   for (ActivityList::const_iterator index = activities_.begin();
	index != activities_.end();
	++index){
      strm << (*index)->getStatus();
   }
   strm << endl;

   if (strategyRepeatCount_ > 1)
   {
      strm << "Strategy repeat countdown: " << strategyRepeatCount_ << endl;
   }

   if (isWrapUpRequested())
   {
      strm << "Wrapping up, waiting for activities"
           << " and any followups to complete..." << endl;
   }

   return strm.str();
}


void ActivityStrategy::restartComponents()
{
   const string msg("Scheduler strategy: restarting components");
   SseArchive::SystemLog() << msg << endl;
   SseArchive::ErrorLog() << msg << endl;

   //const string cmd("restart dxsim1001 dxsim1002 dxsim1003 dxsim1004 dxsim1005 dxsim1006");
   const string cmd("");

   ComponentControlList allComponentControls;
   site_->componentControlManager()->getProxyList(&allComponentControls);

   for (ComponentControlList::iterator componentControl = 
           allComponentControls.begin();
        componentControl != allComponentControls.end(); 
        componentControl++)
   {
      (*componentControl)->sendCommand(cmd);  
   }

   // tbd: use ACE timer to do a wait, then send retry message to self (?)
   // tbd: make wait time a user parameter
   sleep(restartPauseSecs_);

   // TBD error if no component controller is available

}


/*
  Get information from the database on the most recent
  activity failures.  Fetch the data in reverse activity
  order to make it easy to grab the most recent ones,
  and then print the result in reverse to restore
  the correct order.
 */

string ActivityStrategy::getMostRecentFailedActivityInfoFromDb()
{
   if (nssParameters_.db_->useDb())
   {
      stringstream queryText;
      queryText 
         << "select id as ActId, ts as 'StartTime (UTC) ', "
         << "type as Type, "
         << "minDxSkyFreqMhz as MinMhz, maxDxSkyFreqMhz as MaxMhz, "
         << "comments from Activities where validObservation = 'No' "
         << "order by id desc "
         << "limit " 

         // +1 to include the first act that failed
         << maxSequentialFailedActivities_+1;  

      enum resultCols { actCol, tsCol, typeCol, minDxSkyFreqMhzCol,
		     maxDxSkyFreqMhzCol, commentsCol, numCols };
          
      MYSQL * db(nssParameters_.db_->getDb());
      MysqlQuery query(db);
      query.execute(queryText.str(), numCols, __FILE__, __LINE__);
    
      stringstream resultText;
      resultText << SseUtil::currentIsoDateTime() << "\n\n"
                 << "Most recently failed activities: " << "\n\n";

      vector<string> allActLines;

      while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
      {
         stringstream actLine;
         actLine
            << "ActId: " 
            << query.getStringNoThrow(row, actCol, __FILE__, __LINE__)
            
            << " | Time: "
            << query.getStringNoThrow(row, tsCol, __FILE__, __LINE__)
            
            << " | Type: "
            << query.getStringNoThrow(row, typeCol, __FILE__, __LINE__)
            
            << " | MinMHz: "
            << query.getStringNoThrow(row, minDxSkyFreqMhzCol, __FILE__, __LINE__)
            
            << " | MaxMHz: "
            << query.getStringNoThrow(row, maxDxSkyFreqMhzCol, __FILE__, __LINE__)
            
            << " | Comments: "
            << query.getStringNoThrow(row, commentsCol, __FILE__, __LINE__)
            << endl;

         allActLines.push_back(actLine.str());
      }
      
      // print in reverse to make the order right
      for (int i=allActLines.size()-1; i>=0; --i)
      {
         resultText << allActLines[i];
      }

      return resultText.str();
   }

   return "";
}

void ActivityStrategy::sendErrorEmail(const string & msg)
{
   if (nssParameters_.sched_->sendEmailOnStrategyFailure())
   {
      string failureInfo(getMostRecentFailedActivityInfoFromDb());

      const string & subject = msg;
      string body = msg + "\n\n" + failureInfo;
      SseUtil::mailMsg(
	 subject,
	 nssParameters_.sched_->getStrategyFailureEmailAddressList(),
	 body);
   }
}

void ActivityStrategy::attemptToStartNextActivity()
{
   VERBOSE2(getVerboseLevel(), "ActivityStrategy::attemptToStartNextActivity()" << endl;);

   if (isStopRequested())
   {
      strategyFailed();
      return;
   }

   if (isRestartPauseActive())
   {
      return;
   }

   if (isWrapUpRequested())
   {
      const string msg("ActivityStrategy: wrapup in progress...");
      SseArchive::SystemLog() << msg << endl;
      cerr << msg << endl;
   }

   int maxFailures(maxSequentialFailedActivities_);
   int restartWaitSecs(restartPauseSecs_);
   if (waitingForTscopeReady_)
   {
      maxFailures *= tscopeReadyMaxFailures_;
      restartWaitSecs = tscopeReadyWaitIntervalSecs_;
   }

   if (failedStartCount_++ < maxFailures)
   {
      if (startNextActivity())
      {
	 // successful activity start
	 failedStartCount_ = 0;
         waitingForTscopeReady_ = false;
	 return;
      }
      else 
      {
         if (isWrapUpRequested())
         {
            const string msg("Scheduler strategy: act start failed, wrapup requested");
            SseArchive::SystemLog() << msg << endl;
            SseArchive::ErrorLog() << msg << endl;
            cerr << msg << endl;

            stop();
            strategyFailed();
            return;
         }

	 if (restartWaitSecs > 0 && failedStartCount_ < maxFailures)
	 {
            stringstream strm;
            strm << "ActivityStrategy: pausing " << restartWaitSecs << " secs ";
            if (waitingForTscopeReady_)
            {
               strm << "for stable telescope primary beam "
                    << "pointing...";
            }
            else
            {
               strm << "before re-attempting activity start...";
            }
            
	    VERBOSE2(getVerboseLevel(), strm.str() << endl;);
	    cerr << strm.str() << endl;
	    SseArchive::SystemLog() << strm.str() << endl;
	    
	    // TBD try to fix whatever is wrong 
	    //restartComponents();
	    
	    /* Pause to give some time for recovery between attempts,
	       then send a message to self, attempting another restart
	    */
	    restartTimeout_.startTimer(
	    restartWaitSecs,
	    this, &ActivityStrategy::sendWorkAttemptToStartNextActivityMsg,
	    getVerboseLevel());
	    
	 }
	 else
	 {
	    sendWorkAttemptToStartNextActivityMsg();
	 }

      }
   }
   else
   {
      VERBOSE2(getVerboseLevel(), "ActivityStrategy::attemptToStartNextActivity() "
	       << "too many start failures, calling cleanup" << endl;);

      // too many start failures, give up
      const string msg("Scheduler strategy: exceeded max activity restart attempts");
      
      SseArchive::SystemLog() << msg << endl;
      SseArchive::ErrorLog() << msg << endl;
      cerr << msg << endl;

      sendErrorEmail(msg);
      
      strategyFailed();
   }
}

/*
  Returns true if activity was successfully created and started,
  or if that did not happen for a legitimate reason.
  
  Otherwise returns false, indicating an error of some kind.
*/
bool ActivityStrategy::startNextActivity()
{
   VERBOSE2(getVerboseLevel(), "ActivityStrategy::startNextActivity()" << endl;);
cerr << "getting all components" << endl;
   NssComponentTree *nssComponentTree(site_->getAllComponents());
   bool success(false);
   try {
      Activity *activity(getNextActivity(nssComponentTree));
      Assert(activity);
      insertActivity(activity);
      startNextActivityHook(activity);
      
      bool status = activity->start();
      if (status)
      {
         numberActivitiesStarted_++;
         cerr << "Activity " << activity->getId() << " started\n";
      } 
      
      return status;
   }
   catch (ActivityStartWaitingForTargetComplete &except)
   {
      // this is an expected result, so just log it without error
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INFO,
                      SEVERITY_INFO, except.descrip());

      success = true;
   }
   catch (ActivityStartTerminatedDueToWrapupException &except)
   {
      // this is an expected result, so just log it without error
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INFO,
                      SEVERITY_INFO, except.descrip());

      success = true;
   }
   catch (TscopeNotReadyException &except)
   {
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INFO,
                      SEVERITY_INFO, except.descrip());
      
      waitingForTscopeReady_ = true;
      success = false;
   }
   catch (SseException &except)
   {
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, except.code(),
                      except.severity(), except.descrip(), 
                      except.sourceFilename(), except.lineNumber());
      success = false;
   }
   catch (const string & errorText)
   {   
      // don't expect this, but just in case
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INFO,
                      SEVERITY_ERROR, errorText,
                      __FILE__, __LINE__);
      success = false;
   }
   catch (...) 
   {
      // don't expect this, but just in case
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_INFO,
                      SEVERITY_ERROR, "unexpected exception",
                      __FILE__, __LINE__);
      success = false;
   }

   /*
     Assume activity deletes nssComponentTree when finished.
     Since we got here, activity was not created, so remove the tree.
   */
   delete nssComponentTree;

   if (! success)
   {
      SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, SSE_MSG_START_ACT_FAILED,
                      SEVERITY_ERROR, "Failed to create next activity\n",
                      __FILE__, __LINE__);
   }

   return success;
}

void ActivityStrategy::startNextActivityHook(Activity *activity)
{
   // let subclasses override as needed
}


void ActivityStrategy::insertActivity(
   Activity *activity)
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activitiesMutex_);  

   activities_.push_back(activity);
}

void ActivityStrategy::repeatStrategy()
{
   repeatStrategyHook();

   SseArchive::SystemLog() << "Strategy repeat countdown: "
			   << strategyRepeatCount_ << endl;

   attemptToStartNextActivity();
}

void ActivityStrategy::repeatStrategyHook()
{
   // let subclasses override as desired
}


void ActivityStrategy::continueWithAnyMoreActivities()
{
   VERBOSE2(getVerboseLevel(), 
	    "ActivityStrategy::continueWithAnyMoreActivities()" << endl;);

   if (okToStartNewActivity())
   {
      VERBOSE2(getVerboseLevel(), 
	       "ActivityStrategy::continueWithAnyMoreActivities() okToStart" << endl;);

      if (isStopRequested())
      {
	 VERBOSE2(getVerboseLevel(), 
		  "ActivityStrategy::continueWithAnyMoreActivities() stop requested, cleaning up" << endl;);
	 strategyFailed();
	 return;
      }

      if (moreActivitiesToRun())
      {
	 attemptToStartNextActivity();
      }
      else
      {
	 if (numberOfRunningActivities() == 0)
	 {
	    if (--strategyRepeatCount_ > 0)
	    {
	       repeatStrategy();
	    } 
	    else
	    {
	       VERBOSE2(getVerboseLevel(), "ActivityStrategy::continueWithAnyMoreActivities() no more to run, no more repeats, cleaning up" << endl;);
               
	       strategySuccessful();
	    }
	 }
      }
   }
}

void ActivityStrategy::getTscopePrimaryBeamCoords(
      TscopeList &tscopeList, double *ra2000Hours, double *dec2000Deg)
{
   // Get tscope proxy.  Should be one (and only one).
   if (tscopeList.size() < 1)
   {
      throw SseException(
         "no tscope available\n", __FILE__, __LINE__,
         SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
   }
   else if (tscopeList.size() > 1)
   {
      throw SseException(
         "more than one tscope found\n", __FILE__, __LINE__,
         SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);
   }

   TscopeProxy *tscopeProxy = tscopeList.front();

   /*
     Get coordinates for primary beam from tscope status.     
     For now, assume the first synth beam on the assigned list
     has a valid primary beam status associated with it.
     TBD: revisit this.
   */
   vector<TscopeBeam> assignedBeamStatusIndices = 
      site_->getAssignedAtaBeamStatusIndices();

   if (assignedBeamStatusIndices.empty())
   {
      throw SseException(
         "no ata synthesized beams were assigned\n",
         __FILE__, __LINE__,
         SSE_MSG_AUTO_TARG_FAILED, SEVERITY_ERROR);  
   }

   const TscopeStatusMultibeam status = tscopeProxy->getStatus();
   TscopeBeam beamStatusIndex = assignedBeamStatusIndices[0];
   Assert(beamStatusIndex >= 0 && beamStatusIndex < TSCOPE_N_BEAMS);

   /*
      TBD: check that the primary status represents a usable, 
      stable position for selecting targets, e.g.:
      Tracking, not slewing.
      Not in the middle of a calibration.
      etc.
      Add error handling for case when tscope is not ready yet.
      TBD: what to do: wait a bit?  throw an exception?
   */

/*
  // TBD: use other status fields (ant counts) to determine this

   string TrackStateKeyword("TRACK");
   if (status.subarray[beamStatusIndex].driveState != TrackStateKeyword)
   {
      throw TscopeNotReadyException("tscope not currently tracking\n");
   }

*/

   *ra2000Hours = status.primaryPointing[beamStatusIndex].raHours;
   *dec2000Deg =  status.primaryPointing[beamStatusIndex].decDeg;

}


int ActivityStrategy::numberOfRunningActivities()
{
   ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activitiesMutex_);

   return activities_.size();  
}

ActivityId_t ActivityStrategy::getNextActId()
{
   ActivityId_t actId = IdNumberFactory(
      *nssParameters_.db_,
      ActivityIdFilename)->nextId();
   
   return actId;
}

void ActivityStrategy::setMaxObsFreqInParams(NssParameters &nssParams)
{
   if (nssParams.sched_->isAutoTuneRf())
   {
      // do nothing, use existing endObsFreqMhz
   }
   else 
   {
      // User tune - put user selected freq in endObsFreqMhz

      /*
        Assumes that both tuning C & D are in use,
        and enforces that they have the same freq.
        TBD: rework to use configuration data to determine
        which tunings were actually requested.
        
      */
      string tuningCName("tuningc");
      double tuningCFreqMhz(nssParams.tscope_->getTuningSkyFreqMhz(
                               tuningCName));
      
      string tuningDName("tuningd");
      double tuningDFreqMhz(nssParams.tscope_->getTuningSkyFreqMhz(
                               tuningDName));
      
      // check that both freqs are the same
      double freqTolMhz(0.001);
      if (fabs(tuningCFreqMhz - tuningDFreqMhz) > freqTolMhz)
      {
         throw SseException(string("tunings c & d must be set")
                            + " to the same frequency\n", __FILE__,__LINE__);
      }

      nssParams.sched_->setEndObsFreqMhz(tuningCFreqMhz);
   }
}
