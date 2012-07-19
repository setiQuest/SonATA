/*******************************************************************************

 File:    TuneZxs.cpp
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

/*++ TuneZxs.cpp - description of file contents
 * PURPOSE:  
 --*/
// use range or Range
// center frequency, start at edge or center at edge

#include "ace/OS.h"
#include "TuneZxs.h"
#include "DxProxy.h"
#include <MinMaxBandwidth.h>
#include "DxList.h"
#include "Assert.h"
#include "DebugLog.h"
#include "MysqlQuery.h"
#include <map>
#include <sstream>
#include <iostream>
#include <math.h>
#include <algorithm>

using namespace std;


// ------------------------------------
// --- TuneZxs ------------------


TuneZxs::TuneZxs(int verboseLevel):
  verboseLevel_(verboseLevel)
{
}

TuneZxs::~TuneZxs()
{
}

int TuneZxs::getVerboseLevel() const
{
  return verboseLevel_;
}

int TuneZxs::setVerboseLevel(int level)
{
  verboseLevel_ = level;
  return verboseLevel_;
}

void TuneZxs::markZxAsNotInUse(DxProxy *zxProxy)
{
   zxProxy->setDxSkyFreq(-1);
   zxProxy->setChannelNumber(-1);
}

// Tune the zxs to the same sky freq they had 
// in a previous observation.  Extra zxs that are
// in the zxList that were not used previously will
// be ignored.  Freqs that were assigned to zxs that
// are not in the current zxList will not be assigned.

void TuneZxs::tuneZxsFromPrevActInDatabase(
    int oldActivityId, MYSQL* db, DxList & zxList)
{
  const char * zxActParamIdColumn = "dxActivityParametersId";
  const char * zxIntrinIdColumn = "dxIntrinsicsId";

  // take care of any zxs that don't get assigned below
  for (DxList::iterator listIndex = zxList.begin(); 
       listIndex != zxList.end(); ++listIndex)
  {
     markZxAsNotInUse(*listIndex);
  }
    
  stringstream sqlstmt;
  
  sqlstmt 
      << "select DxActivityParameters.dxSkyFrequency as skyfreq, "
     << "DxIntrinsics.dxHostName, "
      << "DxActivityParameters.channelNumber as dxChannel "
      << "from DxActivityParameters, Activities, "
      << "ActivityUnits, DxIntrinsics "
      << "where ActivityUnits." <<  zxActParamIdColumn 
      << "= DxActivityParameters.id "
      << "and ActivityUnits." <<  zxIntrinIdColumn 
      << "= DxIntrinsics.id "
      << "and Activities.id = ActivityUnits.activityId "
      << "and Activities.id = " << oldActivityId << " "
      << "order by skyfreq;";

  enum ColIndices {dxSkyFreqIndex, dxHostNameIndex, channel, numCols};
  
  MysqlQuery query(db);
  query.execute(sqlstmt.str(), numCols, __FILE__, __LINE__);

  // store the previous sky freq for each zx, indexed by name
  typedef map<string, float64_t> ZxSkyFreqAssignMap;
  typedef map<string, int32_t> ZxChannelAssignMap;
  ZxSkyFreqAssignMap skyFreqMap;
  ZxChannelAssignMap channelMap;

  while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
  {
      string origZxHostName(query.getString(row, dxHostNameIndex,
					     __FILE__, __LINE__));
      
      double centerSkyFreqMhz(query.getDouble(row, dxSkyFreqIndex, 
					   __FILE__, __LINE__));
      
      int32_t dxChannel(query.getInt(row, channel, __FILE__, __LINE__));

      skyFreqMap[origZxHostName] = centerSkyFreqMhz;
      channelMap[origZxHostName] = dxChannel;
  }
  
  // now go through the current list of zxs, and look them
  // up by name in the skyFreqMap.  If found, reassign
  // their old frequency.

  DxList::iterator zxIndex;
  for (zxIndex = zxList.begin(); zxIndex != zxList.end();
       ++zxIndex)
  {
    DxProxy *zxProxy = *zxIndex;

    ZxSkyFreqAssignMap::iterator it =
	skyFreqMap.find(zxProxy->getName());
    if (it != skyFreqMap.end())
    {
	double centerSkyFreqMhz = it->second;
	zxProxy->setDxSkyFreq(centerSkyFreqMhz);
    }
    ZxChannelAssignMap::iterator jt =
	channelMap.find(zxProxy->getName());
    if (jt != channelMap.end())
    {
	int32_t assignedChannel = jt->second;
	zxProxy->setChannelNumber(assignedChannel);
    }
  }
}

// ------------------------------------
// --- TuneZxsUser ------------------

TuneZxsUser::TuneZxsUser(int verboseLevel):
  TuneZxs(verboseLevel)
{
}

TuneZxsUser::~TuneZxsUser()
{
}

void TuneZxsUser::tune(DxList &zxList, int32_t totalChannels,
			 float64_t mhzPerChannel, float64_t chanTuneFreqMhz )
{
   // place holder
}
bool TuneZxsUser::moreActivitiesToRun() const
{
   return false;  // only run once
}

// ------------------------------------
// --- TuneZxsAuto --------------------

TuneZxsAuto::TuneZxsAuto(int verboseLevel) :
  TuneZxs(verboseLevel)
{
}

TuneZxsAuto::~TuneZxsAuto()
{
  //delete round_;
}


// ------------------------------------
// --- TuneZxsRange -------------------------

TuneZxsRange::TuneZxsRange(int verboseLevel,
			     const Range& range) :
  TuneZxsAuto(verboseLevel),
  range_(range),
  nextFreq_(range_.low_)
{
}

TuneZxsRange::~TuneZxsRange()
{
}
  // Tune over a single range for SonATA
  // The range_.low_ is used as the left edge of the first channel

void TuneZxsRange::tune(DxList &zxList, int32_t totalChannels,
			 float64_t mhzPerChannel, float64_t chanTuneFreqMhz)
{
 float64_t maxAllowedZxSkyFreqMhz=nextFreq_ + totalChannels*mhzPerChannel;
   float64_t maxZxTuneSeparationMhz = totalChannels*mhzPerChannel;
 
  // set sky freq to dummy value, to mark any zxs not to be used
  for (DxList::iterator zxListIndex = zxList.begin(); 
       zxListIndex != zxList.end(); ++zxListIndex)
  {
     markZxAsNotInUse(*zxListIndex);
  }

   // Get the number of zxs
      int32_t totalZxs = zxList.size();
      int32_t nextChan = (totalChannels - totalZxs)/2;
      if (nextChan < 0) nextChan = 0;


  double firstFreqMhz = nextFreq_;
  int32_t dcChannel = totalChannels/2;
 
  //cout.precision(12);
  //cout << "FirstFreq_" << nextFreq_ << endl;
  //cout << "Chan " << nextChan << " range  low " << range_.low_ << " high " << range_.high_ ;

  for (DxList::iterator zxListIndex = zxList.begin(); 
       zxListIndex != zxList.end();  ++zxListIndex)
  {
    DxProxy *zxProxy = *zxListIndex;

     if ( nextChan == dcChannel)
     {
       // Skip DC
          nextChan++;
          nextFreq_ += mhzPerChannel;
     }
    // nextFreq_ is the lower boundary (left edge sky freq)
    // for the next zx 

    // do not use the remaining PDMs if you run out 
    // of frequencies
    if (nextFreq_ > range_.high_)
    {
	break;
    }

    //float64_t centerFreq = round_->round(calculateCenterFreq(zxProxy));
    // Do not Round !
    float64_t centerFreq = calculateCenterFreq(zxProxy);

    //cout << " centerFreq " << centerFreq << endl;
    // don't exceed the allowed bandwidth
    if ((centerFreq - firstFreqMhz) >= maxZxTuneSeparationMhz)
    {
       break;
    }

    if (centerFreq >= maxAllowedZxSkyFreqMhz)
    {
       break;
    }

    zxProxy->setDxSkyFreq(centerFreq);
    zxProxy->setChannelNumber(nextChan);

    float64_t halfBandwidth = zxProxy->getBandwidthInMHz()/2.0;
    nextFreq_ = centerFreq + halfBandwidth;
    nextChan++;
  }
  
}


float64_t TuneZxsRange::calculateCenterFreq(DxProxy *zxProxy)
{
  return nextFreq_ + zxProxy->getBandwidthInMHz()/2.0;
}


bool TuneZxsRange::moreActivitiesToRun() const
{
   return (nextFreq_ <= range_.high_);
}


// ----------------------------------------------------
// --- TuneZxsForever ----------------------

TuneZxsForever::TuneZxsForever(int verboseLevel):
  TuneZxs(verboseLevel)
{
}

TuneZxsForever::~TuneZxsForever()
{
}

void TuneZxsForever::tune(DxList &zxList, int32_t totalChannels,
			 float64_t mhzPerChannel, float64_t chanTuneFreqMhz )
{
}
bool TuneZxsForever::moreActivitiesToRun() const
{
  return true;
}

// ----------------------------------------------------
// --- TuneZxsObsRange ----------------------

TuneZxsObsRange::TuneZxsObsRange(int verboseLevel,
				   const ObsRange& obsRange) :
  TuneZxsAuto(verboseLevel), 
  obsRange_(obsRange)
{
   Assert(!obsRange.isEmpty());
   nextLeftEdgeFreq_ = obsRange_.minValue();
}


TuneZxsObsRange::~TuneZxsObsRange()
{
}

float64_t TuneZxsObsRange::calculateCenterFreq(DxProxy *zxProxy)
{
    float64_t centerFreq = nextLeftEdgeFreq_ 
	+ zxProxy->getBandwidthInMHz()/2.0;

    return centerFreq;
}



// Try to assign a valid zx tune freq to each zx.
// Unused zxs will be marked as such.
// The first and last zx tune freqs will not be separated by
// greater than maxZxTuneSeparationMhz.
// No zx will be assigned a freq greater than 
// maxAllowedZxSkyFreqMhz.

void TuneZxsObsRange::tune(DxList &zxList, int32_t totalChannels,
			 float64_t mhzPerChannel, float64_t chanTuneFreqMhz )
{

      
 int32_t nextChan; 
  int32_t dcChannel = totalChannels/2;
cout << "dcChannel " << dcChannel << endl;
  float64_t halfBandwidth = mhzPerChannel/2.0;
  float64_t minFreq = chanTuneFreqMhz - dcChannel*mhzPerChannel - halfBandwidth;
cout << "minFreq " << minFreq << endl;
 float64_t maxAllowedZxSkyFreqMhz = chanTuneFreqMhz + 
		dcChannel*mhzPerChannel + halfBandwidth;
cout << "Max ZX Sky freq" << maxAllowedZxSkyFreqMhz << endl;
   float64_t maxZxTuneSeparationMhz = totalChannels*mhzPerChannel;
   VERBOSE2(getVerboseLevel(), "TuneZxsObsRange::tune" 
	    " zxList size = " << zxList.size() 
	    << endl;);

   // take care of any zxs that don't get assigned below
   for (DxList::iterator zxListIndex = zxList.begin(); 
	zxListIndex != zxList.end(); ++zxListIndex)
   {
      markZxAsNotInUse(*zxListIndex);
   }

      Range channelizerRange(minFreq, maxAllowedZxSkyFreqMhz);
      float64_t totalUseableRangeMhz = 
		obsRange_.getUseableBandwidth(channelizerRange);
VERBOSE2(getVerboseLevel(), "totalUseableRangeMhz " << totalUseableRangeMhz << endl);
   // Get the number of zxs
      int32_t totalZxs = zxList.size();
      float64_t totalZxBandwidth = totalZxs*mhzPerChannel;

//  We have to find a bad band in the current range and compute
//  the channel numbe and center frequency

   nextLeftEdgeFreq_ = minFreq;
   nextChan = -1;
   for (DxList::iterator zxListIndex = zxList.begin();
	zxListIndex != zxList.end(); ++zxListIndex)
   {
      // nextLeftEdgeFreq_ is the lower boundary (left edge sky freq)
      // for the next zx 

      DxProxy *zxProxy = *zxListIndex;
      float64_t centerFreq;
    
      do {

	 centerFreq = nextLeftEdgeFreq_ + halfBandwidth;
	 nextLeftEdgeFreq_+= mhzPerChannel;
	nextChan++;
cout << "1centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
cout << "nextChan " << nextChan << endl;

         if ( nextChan == dcChannel)  // skip over
           {
               centerFreq += mhzPerChannel;
               nextLeftEdgeFreq_ += mhzPerChannel;
               nextChan++;
cout << "3centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
cout << "dc Channel " << nextChan << endl;
           }
//cout << "Obs Range " << obsRange_ << endl;

	 if (!obsRange_.isIncluded(nextLeftEdgeFreq_ ))
	 {
// The top edge of the channel is not in the current subrange.
// Find the next range that contains it
	    list<Range>::const_iterator index = 
	       obsRange_.aboveRange(nextLeftEdgeFreq_);
cout << "New Range " << (*index).low_ << "-" << (*index).high_ << endl;
	    if (index == obsRange_.rangeEnd())
	    {
               // Ran out of subranges
	        //cout << "exiting do-while loop, index == obsRange end()\n";
	       break;
	    }
	    else
	    {
// Adjust the nextLeftEdgeFreq_ for the new subrange
// nextLeftEdge has to correspond to a channel edge.

	       DxProxy *firstZxProxy = *zxList.begin();
                  double firstZxFreq = firstZxProxy->getDxSkyFreq();
                  int32_t firstZxChan = firstZxProxy->getChannelNumber();
// Get next Channel Edge in the current subrange
               float64_t bwskip = (((*index).low_ ) + halfBandwidth - nextLeftEdgeFreq_)/mhzPerChannel;
cout << "bwskip " << bwskip << endl;
	       nextLeftEdgeFreq_ += trunc(bwskip+.25)*mhzPerChannel;
		centerFreq = nextLeftEdgeFreq_ + halfBandwidth;
		nextLeftEdgeFreq_ += mhzPerChannel;
cout << "2centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
//cout << "should the channel be incremented here? " << nextChan << endl;
//cout << "5centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;

// Check that the maximum zx separation has not been exceeded.
	       //if (zxProxy != firstZxProxy)
	       {
		  double lowFreqMhz = firstZxFreq - halfBandwidth;
// Compute new channel Number
                  float64_t fnumChan = (nextLeftEdgeFreq_ - minFreq
			+ halfBandwidth)/mhzPerChannel;
                  int32_t numChan = (int32_t)(fnumChan);
// for assume only one zx
                  nextChan =  numChan;
cout << "fnumChan " << fnumChan << " numChan " << numChan << " nextChan " 
	<< nextChan <<  "dcChannel " << endl;
         if ( nextChan == dcChannel)  // skip over
           {
               centerFreq += mhzPerChannel;
               nextLeftEdgeFreq_ += mhzPerChannel;
               nextChan++;
cout << "4centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
cout << "dc Channel " << dcChannel << endl;
           }
	  if ( (nextLeftEdgeFreq_ - minFreq) > maxZxTuneSeparationMhz)
		  {
	      //cout << "exiting do-while loop, zx separation too big\n";
// set zx to channel 0
		nextChan = 0;
		centerFreq = minFreq + halfBandwidth;
		     break;
		  }
	       }
	    }
	 }
      } while (!obsRange_.isIncluded(
	 Range(centerFreq - halfBandwidth + 0.02,
	       centerFreq + halfBandwidth - 0.02)));
#ifdef notForZxs
      if (!obsRange_.isIncluded(
	 Range(centerFreq - halfBandwidth + 0.02,
	       centerFreq + halfBandwidth - 0.02)))
      {
	 cout << "exiting zxlist for loop, range not included\n";
	 break;
      }
#endif
      if (centerFreq > maxAllowedZxSkyFreqMhz)
      {
	 cout << " exiting zxlist for loop, centerfreq too high\n";
	 break;
      }

      zxProxy->setDxSkyFreq(centerFreq);
      zxProxy->setChannelNumber(nextChan);
cout << "CenterFreq " << centerFreq << " nextChan " << nextChan << endl;
         //nextChan++;
cout << "nextChan " << nextChan << endl;

      // don't exceed the maximumZxSeparation for the next zx

      DxProxy *firstZxProxy = *zxList.begin();
      if (zxProxy != firstZxProxy)
      {
	 double highFreqMhz = nextLeftEdgeFreq_ + mhzPerChannel;
	 double lowFreqMhz = firstZxProxy->getDxSkyFreq() - halfBandwidth;
	 
	 if ( (highFreqMhz - lowFreqMhz) > maxZxTuneSeparationMhz)
	 {
	     cout << "exiting zxlist for loop, zx separation too big\n";
	    break;
	 }
	 
      }

   }

} // my TuneZxsObsRange::tune()


bool TuneZxsObsRange::moreActivitiesToRun() const
{
  return (nextLeftEdgeFreq_ != 0);
}
