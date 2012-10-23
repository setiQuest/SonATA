/*******************************************************************************

 File:    CalDelayActStrategy.cpp
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



#include "CalDelayActStrategy.h" 
#include "Interpolate.h"

static const double CalFreqMhz(1420);

CalDelayActStrategy::CalDelayActStrategy(
                               Scheduler *scheduler,
                               Site *site, 
                               const NssParameters &nssParameters,
                               int verboseLevel):
   CalActStrategy(scheduler, site, nssParameters, verboseLevel)
{
}

CalDelayActStrategy::~CalDelayActStrategy()
{
}

void CalDelayActStrategy::loadCalFreqListMhz()
{
   addCalFreqMhz(CalFreqMhz);
}

string CalDelayActStrategy::getCatalogName()
{
   return "caldelay";
}

string CalDelayActStrategy::getCalType()
{
   return "delay";
}

int CalDelayActStrategy::getNumCalCycles()
{
   return 3;
}

int CalDelayActStrategy::getCalTimeSecs(double obsFreqMhz, double targetFluxJy)
{
   const string BfCalTimeTable("BfCalDelayTime");
   return computeCalTimeSecs(BfCalTimeTable, obsFreqMhz, targetFluxJy);
}



