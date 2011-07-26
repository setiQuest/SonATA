/*******************************************************************************

 File:    TuneDxs.cpp
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

/*++ TuneDxs.cpp - description of file contents
 * PURPOSE:  
 --*/
// use range or Range
// center frequency, start at edge or center at edge

#include "ace/OS.h"
#include "TuneDxs.h"
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
// --- Round -------------------------

class Round 
{
public:
  virtual float64_t round(float64_t freq) = 0;
  virtual ~Round();
};

Round::~Round()
{
}

// ------------------------------------
// --- RoundNoRound ------------------

class RoundNoRound : public Round
{
public:
  virtual float64_t round(float64_t freq);
  virtual ~RoundNoRound();
};

RoundNoRound::~RoundNoRound()
{
}

float64_t RoundNoRound::round(float64_t freq)
{
  return freq;
}
  
// ------------------------------------
// --- RoundToValue ------------------

class RoundToValue : public Round
{
public:
  RoundToValue(float64_t roundValue);
  virtual ~RoundToValue();
  virtual float64_t round(float64_t freq);
protected:
  float64_t roundValue_;
};

RoundToValue::~RoundToValue()
{
}
  
RoundToValue::RoundToValue(float64_t roundValue):
  roundValue_(roundValue)
{
}

// rounds downward to nearest multiple of roundValue_,
// unless roundValue_ is near 0, in which case freq is returned
float64_t RoundToValue::round(float64_t freq)
{
  float64_t result = freq;

  // this should be more than sufficient,
  // as it's well below Hz tolerance
  const double minRoundValue = 1e-30;  

  if (fabs(roundValue_) >= minRoundValue)
  {
      result = floor(freq/roundValue_) * roundValue_ ;
  }

  return result;
}

// ------------------------------------
// --- TuneDxs ------------------


TuneDxs::TuneDxs(int verboseLevel):
  verboseLevel_(verboseLevel)
{
}

TuneDxs::~TuneDxs()
{
}

int TuneDxs::getVerboseLevel() const
{
  return verboseLevel_;
}

int TuneDxs::setVerboseLevel(int level)
{
  verboseLevel_ = level;
  return verboseLevel_;
}

void TuneDxs::markDxAsNotInUse(DxProxy *dxProxy)
{
   dxProxy->setDxSkyFreq(-1);
   dxProxy->setChannelNumber(-1);
}

// Tune the dxs to the same sky freq they had 
// in a previous observation.  Extra dxs that are
// in the dxList that were not used previously will
// be ignored.  Freqs that were assigned to dxs that
// are not in the current dxList will not be assigned.

void TuneDxs::tuneDxsFromPrevActInDatabase(
    int oldActivityId, MYSQL* db, DxList & dxList)
{
  const char * dxActParamIdColumn = "dxActivityParametersId";
  const char * dxIntrinIdColumn = "dxIntrinsicsId";

  // take care of any dxs that don't get assigned below
  for (DxList::iterator listIndex = dxList.begin(); 
       listIndex != dxList.end(); ++listIndex)
  {
     markDxAsNotInUse(*listIndex);
  }
    
  stringstream sqlstmt;
  
  sqlstmt 
      << "select DxActivityParameters.dxSkyFrequency as skyfreq, "
     << "DxIntrinsics.dxHostName, "
      << "DxActivityParameters.channelNumber as dxChannel "
      << "from DxActivityParameters, Activities, "
      << "ActivityUnits, DxIntrinsics "
      << "where ActivityUnits." <<  dxActParamIdColumn 
      << "= DxActivityParameters.id "
      << "and ActivityUnits." <<  dxIntrinIdColumn 
      << "= DxIntrinsics.id "
      << "and Activities.id = ActivityUnits.activityId "
      << "and Activities.id = " << oldActivityId << " "
      << "order by skyfreq;";

  enum ColIndices {dxSkyFreqIndex, dxHostNameIndex, channel, numCols};
  
  MysqlQuery query(db);
  query.execute(sqlstmt.str(), numCols, __FILE__, __LINE__);

  // store the previous sky freq for each dx, indexed by name
  typedef map<string, float64_t> DxSkyFreqAssignMap;
  typedef map<string, int32_t> DxChannelAssignMap;
  DxSkyFreqAssignMap skyFreqMap;
  DxChannelAssignMap channelMap;

  while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
  {
      string origDxHostName(query.getString(row, dxHostNameIndex,
					     __FILE__, __LINE__));
      
      double centerSkyFreqMhz(query.getDouble(row, dxSkyFreqIndex, 
					   __FILE__, __LINE__));
      
      int32_t dxChannel(query.getInt(row, channel, __FILE__, __LINE__));

      skyFreqMap[origDxHostName] = centerSkyFreqMhz;
      channelMap[origDxHostName] = dxChannel;
  }
  
  // now go through the current list of dxs, and look them
  // up by name in the skyFreqMap.  If found, reassign
  // their old frequency.

  DxList::iterator dxIndex;
  for (dxIndex = dxList.begin(); dxIndex != dxList.end();
       ++dxIndex)
  {
    DxProxy *dxProxy = *dxIndex;

    DxSkyFreqAssignMap::iterator it =
	skyFreqMap.find(dxProxy->getName());
    if (it != skyFreqMap.end())
    {
	double centerSkyFreqMhz = it->second;
	dxProxy->setDxSkyFreq(centerSkyFreqMhz);
    }
    DxChannelAssignMap::iterator jt =
	channelMap.find(dxProxy->getName());
    if (jt != channelMap.end())
    {
	int32_t assignedChannel = jt->second;
	dxProxy->setChannelNumber(assignedChannel);
    }
  }
}

// ------------------------------------
// --- TuneDxsUser ------------------

TuneDxsUser::TuneDxsUser(int verboseLevel):
  TuneDxs(verboseLevel)
{
}

TuneDxsUser::~TuneDxsUser()
{
}

void TuneDxsUser::tune(DxList &dxList, int32_t totalChannels,
			 float64_t mhzPerChannel )
{
   // place holder
}
void TuneDxsUser::tune(DxList &dxList, float64_t maxDxTuneSeparationMhz,
			 float64_t maxAllowedDxSkyFreqMhz)
{
   // nothing is done here.  Current settings are used.
}

bool TuneDxsUser::moreActivitiesToRun() const
{
   return false;  // only run once
}

// ------------------------------------
// --- TuneDxsAuto --------------------

TuneDxsAuto::TuneDxsAuto(int verboseLevel) :
  TuneDxs(verboseLevel)
{
}

TuneDxsAuto::TuneDxsAuto(int verboseLevel,
			   float64_t overlap,
			   float64_t tuningTolerance,
			   Round* round):
  TuneDxs(verboseLevel),
  overlap_(overlap),
  tuningTolerance_(tuningTolerance),
  round_(round)
{
}

TuneDxsAuto::~TuneDxsAuto()
{
  //delete round_;
}

float64_t TuneDxsAuto::tuningTolerance() const
{
  return tuningTolerance_;
}

float64_t TuneDxsAuto::overlap(float64_t skyFreq) const
{
  return overlap_;
}

// ------------------------------------
// --- TuneDxsRange -------------------------

TuneDxsRange::TuneDxsRange(int verboseLevel,
			     const Range& range) :
  TuneDxsAuto(verboseLevel),
  range_(range),
  nextFreq_(range_.low_)
{
}

TuneDxsRange::TuneDxsRange(int verboseLevel,
			     const Range& range,
			     float64_t overlap,
			     float64_t tuningTolerance,
			     Round* round):
  TuneDxsAuto(verboseLevel, overlap, tuningTolerance, round),
  range_(range),
  nextFreq_(range_.low_)
{
}

TuneDxsRange::TuneDxsRange(int verboseLevel,
			     const Range& range,
			     float64_t overlap,
			     float64_t tuningTolerance):
  TuneDxsAuto(verboseLevel, overlap,
	       tuningTolerance, new RoundNoRound()),
  range_(range),
  nextFreq_(range_.low_)
{
}

TuneDxsRange::~TuneDxsRange()
{
}
  // Tune over a single range for SonATA
  // The range_.low_ is used as the left edge of the first channel

void TuneDxsRange::tune(DxList &dxList, int32_t totalChannels,
			 float64_t mhzPerChannel)
{
 float64_t maxAllowedDxSkyFreqMhz=nextFreq_ + totalChannels*mhzPerChannel;
   float64_t maxDxTuneSeparationMhz = totalChannels*mhzPerChannel;
 
  // set sky freq to dummy value, to mark any dxs not to be used
  for (DxList::iterator dxListIndex = dxList.begin(); 
       dxListIndex != dxList.end(); ++dxListIndex)
  {
     markDxAsNotInUse(*dxListIndex);
  }

   // Get the number of dxs
      int32_t totalDxs = dxList.size();
      int32_t nextChan = (totalChannels - totalDxs)/2;
      if (nextChan < 0) nextChan = 0;


  double firstFreqMhz = nextFreq_;
  int32_t dcChannel = totalChannels/2;
 
  //cout.precision(12);
  //cout << "FirstFreq_" << nextFreq_ << endl;
  //cout << "Chan " << nextChan << " range  low " << range_.low_ << " high " << range_.high_ ;

  for (DxList::iterator dxListIndex = dxList.begin(); 
       dxListIndex != dxList.end();  ++dxListIndex)
  {
    DxProxy *dxProxy = *dxListIndex;

     if ( nextChan == dcChannel)
     {
       // Skip DC
          nextChan++;
          nextFreq_ += mhzPerChannel;
     }
    // nextFreq_ is the lower boundary (left edge sky freq)
    // for the next dx 

    // do not use the remaining PDMs if you run out 
    // of frequencies
    if (nextFreq_ > range_.high_)
    {
	break;
    }

    //float64_t centerFreq = round_->round(calculateCenterFreq(dxProxy));
    // Do not Round !
    float64_t centerFreq = calculateCenterFreq(dxProxy);

    //cout << " centerFreq " << centerFreq << endl;
    // don't exceed the allowed bandwidth
    if ((centerFreq - firstFreqMhz) >= maxDxTuneSeparationMhz)
    {
       break;
    }

    if (centerFreq >= maxAllowedDxSkyFreqMhz)
    {
       break;
    }

    dxProxy->setDxSkyFreq(centerFreq);
    dxProxy->setChannelNumber(nextChan);

    float64_t halfBandwidth = dxProxy->getBandwidthInMHz()/2.0;
    nextFreq_ = centerFreq + halfBandwidth;
    nextChan++;
  }
  
}

  // Tune over a single range
  // The range_.low_ is used as the left edge of the first channel

void TuneDxsRange::tune(DxList &dxList, float64_t maxDxTuneSeparationMhz,
			 float64_t maxAllowedDxSkyFreqMhz )
{
   VERBOSE2(getVerboseLevel(), "TuneDxsRange::tune" 
	    " dxList size = " << dxList.size() 
	    << " maxDxTuneSeparationMhz = " << maxDxTuneSeparationMhz 
	    << " maxAllowedDxSkyFreqMhz = " << maxAllowedDxSkyFreqMhz
	    << endl;);
    
  // set sky freq to dummy value, to mark any dxs not to be used
  for (DxList::iterator dxListIndex = dxList.begin(); 
       dxListIndex != dxList.end(); ++dxListIndex)
  {
     markDxAsNotInUse(*dxListIndex);
  }

  double firstFreqMhz = nextFreq_;

  for (DxList::iterator dxListIndex = dxList.begin(); 
       dxListIndex != dxList.end();  ++dxListIndex)
  {
    DxProxy *dxProxy = *dxListIndex;

    // nextFreq_ is the lower boundary (left edge sky freq)
    // for the next dx 

    // do not use the remaining PDMs if you run out 
    // of frequencies
    if (nextFreq_ > range_.high_)
    {
	break;
    }

    //float64_t centerFreq = round_->round(calculateCenterFreq(dxProxy));
    // Do not Round !
    float64_t centerFreq = calculateCenterFreq(dxProxy);

    // don't exceed the allowed bandwidth
    if ((centerFreq - firstFreqMhz) >= maxDxTuneSeparationMhz)
    {
       break;
    }

    if (centerFreq >= maxAllowedDxSkyFreqMhz)
    {
       break;
    }

    dxProxy->setDxSkyFreq(centerFreq);

    float64_t halfBandwidth = dxProxy->getBandwidthInMHz()/2.0;
    nextFreq_ = centerFreq + halfBandwidth;
  }
  
}

float64_t TuneDxsRange::calculateCenterFreq(DxProxy *dxProxy)
{
  return nextFreq_ + dxProxy->getBandwidthInMHz()/2.0;
}


bool TuneDxsRange::moreActivitiesToRun() const
{
   return (nextFreq_ <= range_.high_);
}


// ----------------------------------------------------
// --- TuneDxsRangeCenter ----------------------------

TuneDxsRangeCenter::TuneDxsRangeCenter(int verboseLevel,
					 const Range& range,
					 float64_t overlap,
					 float64_t tuningTolerance,
					 Round* round) :
  TuneDxsRange(verboseLevel, range, overlap, tuningTolerance, round)
{
}

TuneDxsRangeCenter::~TuneDxsRangeCenter()
{
}

TuneDxsRangeCenter::TuneDxsRangeCenter(int verboseLevel,
					 const Range& range,
					 float64_t overlap,
					 float64_t tuningTolerance) :
  TuneDxsRange(verboseLevel, range, overlap, tuningTolerance)
{
}

TuneDxsRangeCenter::TuneDxsRangeCenter(int verboseLevel,
					 const Range& range) :
  TuneDxsRange(verboseLevel, range)
{
}

float64_t TuneDxsRangeCenter::calculateCenterFreq(DxProxy *dxProxy)
{
    return TuneDxsRange::calculateCenterFreq(dxProxy);
}

// ----------------------------------------------------
// --- TuneDxsRangeCenterRound ----------------------------

TuneDxsRangeCenterRound::TuneDxsRangeCenterRound(int verboseLevel,
						   const Range& range,
						   float64_t roundValue,
						   float64_t overlap,
						   float64_t tuningTolerance) :
  TuneDxsRangeCenter(verboseLevel, range, overlap, tuningTolerance,
		      new RoundToValue(roundValue))
{
}

// ----------------------------------------------------
// --- TuneDxsForever ----------------------

TuneDxsForever::TuneDxsForever(int verboseLevel):
  TuneDxs(verboseLevel)
{
}

TuneDxsForever::~TuneDxsForever()
{
}

void TuneDxsForever::tune(DxList &dxList, int32_t totalChannels,
			 float64_t mhzPerChannel )
{
}
void TuneDxsForever::tune(DxList &dxList, float64_t maxDxTuneSeparationMhz,
			   float64_t maxAllowedDxSkyFreqMhz )
{
    // do nothing
}

bool TuneDxsForever::moreActivitiesToRun() const
{
  return true;
}

// ----------------------------------------------------
// --- TuneDxsObsRange ----------------------

TuneDxsObsRange::TuneDxsObsRange(int verboseLevel,
				   const ObsRange& obsRange) :
  TuneDxsAuto(verboseLevel), 
  obsRange_(obsRange)
{
   Assert(!obsRange.isEmpty());
   nextLeftEdgeFreq_ = obsRange_.minValue();
}

TuneDxsObsRange::TuneDxsObsRange(int verboseLevel,
				   const ObsRange& obsRange,
				   float64_t roundValue,
				   float64_t overlap,
				   float64_t tuningTolerance):
  TuneDxsAuto(verboseLevel, overlap,
	       tuningTolerance, new RoundToValue(roundValue)),
  obsRange_(obsRange)
{
   Assert(!obsRange.isEmpty());
   nextLeftEdgeFreq_ = obsRange_.minValue();
}

TuneDxsObsRange::~TuneDxsObsRange()
{
}

float64_t TuneDxsObsRange::calculateCenterFreq(DxProxy *dxProxy)
{
    float64_t centerFreq = nextLeftEdgeFreq_ 
	+ dxProxy->getBandwidthInMHz()/2.0;

    return centerFreq;
}



// Try to assign a valid dx tune freq to each dx.
// Unused dxs will be marked as such.
// The first and last dx tune freqs will not be separated by
// greater than maxDxTuneSeparationMhz.
// No dx will be assigned a freq greater than 
// maxAllowedDxSkyFreqMhz.

void TuneDxsObsRange::tune(DxList &dxList, int32_t totalChannels,
			 float64_t mhzPerChannel )
{
 int32_t nextChan; 
 float64_t maxAllowedDxSkyFreqMhz =
		nextLeftEdgeFreq_ + totalChannels*mhzPerChannel;
   float64_t maxDxTuneSeparationMhz = totalChannels*mhzPerChannel;
   VERBOSE2(getVerboseLevel(), "TuneDxsObsRange::tune" 
	    " dxList size = " << dxList.size() 
	    << " maxDxTuneSeparationMhz = " << maxDxTuneSeparationMhz 
	    << " maxAllowedDxSkyFreqMhz = " << maxAllowedDxSkyFreqMhz
	    << endl;);

   // take care of any dxs that don't get assigned below
   for (DxList::iterator dxListIndex = dxList.begin(); 
	dxListIndex != dxList.end(); ++dxListIndex)
   {
      markDxAsNotInUse(*dxListIndex);
   }

      Range channelizerRange(nextLeftEdgeFreq_, maxAllowedDxSkyFreqMhz);
      float64_t totalUseableRangeMhz = 
		obsRange_.getUseableBandwidth(channelizerRange);
VERBOSE2(getVerboseLevel(), "totalUseableRangeMhz " << totalUseableRangeMhz << endl);
   // Get the number of dxs
      int32_t totalDxs = dxList.size();
      float64_t totalDxBandwidth = totalDxs*mhzPerChannel;


  int32_t dcChannel = totalChannels/2;
//cout << "dcChannel " << dcChannel << endl;
  float64_t halfBandwidth = mhzPerChannel/2.0;


// For now always start at channel zero, because it's much easier!
      nextChan = -1;

   for (DxList::iterator dxListIndex = dxList.begin();
	dxListIndex != dxList.end(); ++dxListIndex)
   {
      // nextLeftEdgeFreq_ is the lower boundary (left edge sky freq)
      // for the next dx 

      DxProxy *dxProxy = *dxListIndex;
      float64_t centerFreq;
    
      do {

	 centerFreq = nextLeftEdgeFreq_ + halfBandwidth;
	 nextLeftEdgeFreq_+= mhzPerChannel;
	nextChan++;
//cout << "1centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
//cout << "nextChan " << nextChan << endl;

         if ( nextChan == dcChannel)  // skip over
           {
               centerFreq += mhzPerChannel;
               nextLeftEdgeFreq_ += mhzPerChannel;
               nextChan++;
//cout << "3centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
//cout << "dc Channel " << nextChan << endl;
           }
//cout << "Obs Range " << obsRange_ << endl;

	 if (!obsRange_.isIncluded(nextLeftEdgeFreq_ ))
	 {
// The top edge of the channel is not in the current subrange.
// Find the next range that contains it
	    list<Range>::const_iterator index = 
	       obsRange_.aboveRange(nextLeftEdgeFreq_);
//cout << "New Range " << (*index).low_ << "-" << (*index).high_ << endl;
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

	       DxProxy *firstDxProxy = *dxList.begin();
                  double firstDxFreq = firstDxProxy->getDxSkyFreq();
                  int32_t firstDxChan = firstDxProxy->getChannelNumber();
// Get next Channel Edge in the current subrange
               float64_t bwskip = (((*index).low_ ) + halfBandwidth - nextLeftEdgeFreq_)/mhzPerChannel;
//cout << "bwskip " << bwskip << endl;
	       nextLeftEdgeFreq_ += trunc(bwskip+.25)*mhzPerChannel;
		centerFreq = nextLeftEdgeFreq_ + halfBandwidth;
//cout << "2centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
		nextLeftEdgeFreq_ += mhzPerChannel;
//cout << "should the channel be incremented here? " << nextChan << endl;
//cout << "5centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;

// Check that the maximum dx separation has not been exceeded.
	       if (dxProxy != firstDxProxy)
	       {
		  double lowFreqMhz = firstDxFreq - halfBandwidth;
// Compute new channel Number
                  float64_t fnumChan = (nextLeftEdgeFreq_ - firstDxFreq
			+ halfBandwidth)/mhzPerChannel;
                  int32_t numChan = (int32_t)(fnumChan);
                  nextChan = firstDxChan + numChan;
//cout << "fnumChan " << fnumChan << " numChan " << numChan << " nextChan " 
	//<< nextChan << " firstDxChan " << firstDxChan << endl;
         if ( nextChan == dcChannel)  // skip over
           {
               centerFreq += mhzPerChannel;
               nextLeftEdgeFreq_ += mhzPerChannel;
               nextChan++;
//cout << "4centerFreq " << centerFreq << " nextLeftEdgeFreq " << nextLeftEdgeFreq_ << endl;
//cout << "dc Channel " << nextChan << endl;
           }
	  if ( (nextLeftEdgeFreq_ - lowFreqMhz) > maxDxTuneSeparationMhz)
		  {
	      //cout << "exiting do-while loop, dx separation too big\n";
		     break;
		  }
	       }
	    }
	 }
      } while (!obsRange_.isIncluded(
	 Range(centerFreq - halfBandwidth + 0.02,
	       centerFreq + halfBandwidth - 0.02)));

      if (!obsRange_.isIncluded(
	 Range(centerFreq - halfBandwidth + 0.02,
	       centerFreq + halfBandwidth - 0.02)))
      {
	 //cout << "exiting dxlist for loop, range not included\n";
	 break;
      }

      if (centerFreq > maxAllowedDxSkyFreqMhz)
      {
	 //cout << " exiting dxlist for loop, centerfreq too high\n";
	 break;
      }

      dxProxy->setDxSkyFreq(centerFreq);
      dxProxy->setChannelNumber(nextChan);
//cout << "CenterFreq " << centerFreq << " nextChan " << nextChan << endl;
         //nextChan++;
//cout << "nextChan " << nextChan << endl;

      // don't exceed the maximumDxSeparation for the next dx

      DxProxy *firstDxProxy = *dxList.begin();
      if (dxProxy != firstDxProxy)
      {
	 double highFreqMhz = nextLeftEdgeFreq_ + mhzPerChannel;
	 double lowFreqMhz = firstDxProxy->getDxSkyFreq() - halfBandwidth;
	 
	 if ( (highFreqMhz - lowFreqMhz) > maxDxTuneSeparationMhz)
	 {
	     //cout << "exiting dxlist for loop, dx separation too big\n";
	    break;
	 }
	 
      }

   }

} // my TuneDxsObsRange::tune()

void TuneDxsObsRange::tune(DxList &dxList,
			    float64_t maxDxTuneSeparationMhz,
			    float64_t maxAllowedDxSkyFreqMhz)
{
   VERBOSE2(getVerboseLevel(), "TuneDxsObsRange::tune" 
	    " dxList size = " << dxList.size() 
	    << " maxDxTuneSeparationMhz = " << maxDxTuneSeparationMhz 
	    << " maxAllowedDxSkyFreqMhz = " << maxAllowedDxSkyFreqMhz
	    << endl;);

   // take care of any dxs that don't get assigned below
   for (DxList::iterator dxListIndex = dxList.begin(); 
	dxListIndex != dxList.end(); ++dxListIndex)
   {
      markDxAsNotInUse(*dxListIndex);
   }

 
   for (DxList::iterator dxListIndex = dxList.begin();
	dxListIndex != dxList.end(); ++dxListIndex)
   {
      // nextLeftEdgeFreq_ is the lower boundary (left edge sky freq)
      // for the next dx 

      DxProxy *dxProxy = *dxListIndex;
      float64_t halfBandwidth = dxProxy->getBandwidthInMHz()/2.0;
      float64_t centerFreq;
    
      do {

	 centerFreq = round_->round(calculateCenterFreq(dxProxy));
	 nextLeftEdgeFreq_ = centerFreq + halfBandwidth;

	 //cout << "centerfreq=" << centerFreq ;
	 //cout << " 	lower " << centerFreq - halfBandwidth +
	 //overlap(centerFreq) + tuningTolerance() + 0.1
	 //<< " Upper " << centerFreq + halfBandwidth - 0.1
	 //<< endl;
	 //cout << "nextLeftEdgeFreq_=" << nextLeftEdgeFreq_ << endl;

	 if (!obsRange_.isIncluded(nextLeftEdgeFreq_ ))
	 {
	    list<Range>::const_iterator index = 
	       obsRange_.aboveRange(nextLeftEdgeFreq_);
	    if (index == obsRange_.rangeEnd())
	    {
	       // cout << "exiting do-while loop, index == obsRange end()\n";
	       break;
	    }
	    else
	    {
	       nextLeftEdgeFreq_ = (*index).low_;
	       //cout <<"obsrange nextLeftEdgeFreq_=" << nextLeftEdgeFreq_ << endl;

	       DxProxy *firstDxProxy = *dxList.begin();
	       if (dxProxy != firstDxProxy)
	       {
		  double highFreqMhz = nextLeftEdgeFreq_ + dxProxy->getBandwidthInMHz();
		  double lowFreqMhz = firstDxProxy->getDxSkyFreq() -
		     firstDxProxy->getBandwidthInMHz()/2.0;

		  if ( (highFreqMhz - lowFreqMhz) > maxDxTuneSeparationMhz)
		  {
		     // cout << "exiting do-while loop, dx separation too big\n";
		     break;
		  }
	       }
	    }
	 }
      } while (!obsRange_.isIncluded(
	 Range(centerFreq - halfBandwidth + 0.1,
	       centerFreq + halfBandwidth - 0.1)));

      if (!obsRange_.isIncluded(
	 Range(centerFreq - halfBandwidth + 0.1,
	       centerFreq + halfBandwidth - 0.1)))
      {
	 //cout << "exiting dxlist for loop, range not included\n";
	 break;
      }

      if (centerFreq > maxAllowedDxSkyFreqMhz)
      {
	 //cout << " exiting dxlist for loop, centerfreq too high\n";
	 break;
      }

      dxProxy->setDxSkyFreq(centerFreq);
      

      // don't exceed the maximumDxSeparation

      DxProxy *firstDxProxy = *dxList.begin();
      if (dxProxy != firstDxProxy)
      {
	 double highFreqMhz = nextLeftEdgeFreq_ + dxProxy->getBandwidthInMHz();
	 double lowFreqMhz = firstDxProxy->getDxSkyFreq() -
	    firstDxProxy->getBandwidthInMHz()/2.0;
	 
	 if ( (highFreqMhz - lowFreqMhz) > maxDxTuneSeparationMhz)
	 {
	    // cout << "exiting dxlist for loop, dx separation too big\n";
	    break;
	 }
	 
      }

   }

}

bool TuneDxsObsRange::moreActivitiesToRun() const
{
  return (nextLeftEdgeFreq_ != 0);
}
