/*******************************************************************************

 File:    BfActStrategy.cpp
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



#include "BfActStrategy.h" 
#include "ActivityWrappers.h"
#include "ActParameters.h"
#include "SchedulerParameters.h"
#include "Scheduler.h"

BfActStrategy::BfActStrategy(Scheduler *scheduler,
                             Site *site, 
                             const NssParameters &nssParameters,
                             const string &actType,
                             int verboseLevel):
   ActivityStrategy(scheduler, site, nssParameters, verboseLevel),
   actType_(actType),
   nssParameters_(nssParameters),
   tryAgain_(false)
{
}

BfActStrategy::~BfActStrategy()
{
}

NssParameters & BfActStrategy::getNssParameters()
{
   return nssParameters_;
}

Activity * BfActStrategy::getNextActivity(
   NssComponentTree *nssComponentTree)
{
   string methodName("BfActStrategy::getNextActivity");

   // set activity type (for logging purposes)
   if (! nssParameters_.act_->setActivityType(actType_))
   {
      throw SseException("tried to set invalid activity type: " + actType_
                         + " in " + methodName + "\n", __FILE__, __LINE__);
   }    

   Activity *act = createNewActivity(nssComponentTree);

   return act;
}

bool BfActStrategy::moreActivitiesToRun()
{
   return tryAgain_;
}

bool BfActStrategy::okToStartNewActivity()
{
   // This needs to be true for the strategy to wrap up

   return true;
}

void BfActStrategy::activityCompleteInternalHook(
   Activity *activity, bool failed)
{
   tryAgain_ = failed;
   if (failed ) Scheduler::instance()->stop();
}

