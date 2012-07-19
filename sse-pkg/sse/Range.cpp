/*******************************************************************************

 File:    Range.cpp
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

/*++ Range.cpp - description of file contents
 * Classes which store ranges of numbers
 * PURPOSE:  
 --*/

#include "Range.h"
#include "sseDxInterface.h"
#include "Assert.h"
#include <iomanip>
#include <algorithm>
#include <sstream>

using namespace std;

static const int RangePrintPrecision(6);  // nearest Hz

ObsRange::ObsRange()
{
}


ObsRange::~ObsRange()
{
}

Range::Range(float64_t low, float64_t high):
  low_(low), high_(high)
{
}

float64_t Range::totalRange() const
{
  return high_ - low_;
}

Range::~Range()
{}


class FindContainingRange
{
public:
  FindContainingRange(float64_t minValue):
    minValue_(minValue)
  {
  }
  bool operator()(const Range & range)
   {
      if ((range.low_ <= minValue_) && (minValue_ <= range.high_)){
	 return true;
      }
      else {
	 return false;
      }
   }
protected:
   float64_t minValue_;
};

class FindLessThanRange
{
public:
   FindLessThanRange(float64_t minValue):
      minValue_(minValue)
   {
   }
   bool operator()(const Range & range)
   {
      if (range.high_ >= minValue_){
	 return true;
      }
      else {
	 return false;
      }
   }
protected:
   float64_t minValue_;
};


// assume that low is greater than all previous lows
void ObsRange::addInOrder(float64_t low, float64_t high)
{
   if (ranges_.empty())
   {
      ranges_.push_back(Range(low, high));
      return;
   }

   Range & range = *ranges_.rbegin(); 
   if (low <= range.high_)
   {
      range.high_ = max(range.high_, high);
   }
   else
   {
      ranges_.push_back(Range(low, high));
   }
}

// do not assume that low is greater than all previous lows
void ObsRange::addOutOfOrder(float64_t low, float64_t high)
{
   if (ranges_.empty())
   {
      ranges_.push_back(Range(low, high));
      return;
   }

   list<Range>::iterator index = aboveRange(low);
   if (index == ranges_.end())
   {
      ranges_.push_back(Range(low, high));
      return;
   }
  
   if (low <= (*index).low_)
   {
      if (high >= (*index).low_)
      {
	 (*index).high_ = max((*index).high_, high);
	 (*index).low_ = low;
      }
      else
      {
	 if (index == ranges_.begin())
	 {
	    ranges_.push_front(Range(low, high));
	 }
	 else
	 {
	    // insert before the next higher range
	    ranges_.insert(index, Range(low, high));
	 }
      }
      
   }
   else
   {
      (*index).high_ = max((*index).high_, high);
   }

   merge();

}


void ObsRange::merge()
{
   list<Range>::iterator upper = ranges_.begin();
   if (upper == ranges_.end())
   {
      return;
   }
  
   for (list<Range>::iterator lower = upper++;
	upper != ranges_.end();
	lower = upper, ++upper)
   {
      while (lower != ranges_.end() &&
	     upper != ranges_.end() &&
	     (*lower).high_ >= (*upper).low_)
      {
	 (*lower).low_ = min((*lower).low_, (*upper).low_);
	 (*lower).high_ = max((*lower).high_, (*upper).high_);

	 ranges_.erase(upper);

	 upper = lower;
	 upper++;
      }

      if (upper == ranges_.end())
      {
	 break;
      }
   }
}


// is value included in the range of one of the Ranges?
bool ObsRange::isIncluded(float64_t value) const
{
   for (list<Range>::const_iterator i = ranges_.begin();
	i != ranges_.end(); ++i)
   {
      if ((value >= (*i).low_) && (value <= (*i).high_))
      {
	 return true;
      }
   }

   return false;
}

bool ObsRange::isIncluded(const Range& range) const
{
   for (list<Range>::const_iterator i = ranges_.begin();
	i != ranges_.end(); ++i)
   {
      if ((range.low_ >= (*i).low_) && (range.low_ <= (*i).high_))
      {
	 if ((range.high_ >= (*i).low_) && (range.high_ <= (*i).high_))
	 {
	    return true;
	 }
	 else
	 {
	    return false;
	 }
      }
   }

   return false;
}

float64_t ObsRange::getUseableBandwidth(const Range& range) const
{
   float64_t useableBandwidth = 0.0;

   for (list<Range>::const_iterator i = ranges_.begin();
	i != ranges_.end(); ++i)
   {
      if (( (*i).low_ >= range.low_ ) && ((*i).high_ <= range.high_))
                  useableBandwidth += ((*i).high_ - (*i).low_);
      else if ( ((*i).low_ <= range.low_) && ((*i).high_ <= range.high_))
                  useableBandwidth += ((*i).high_ - range.low_);
      else if ( ((*i).low_ >= range.low_) && ((*i).high_ >= range.high_))
                  useableBandwidth += ( range.high_ - (*i).low_);
// In the case that both (*i),low_ and (*i),high_ are below range.low_
// and the case that both (*i),low_ and (*i),high_ are above range.high_
// do nothing
   }
      return useableBandwidth;

}


// first range which is greater than or equal to value
list<Range>::const_iterator ObsRange::aboveRange(float64_t value) const
{
   return find_if(ranges_.begin(), ranges_.end(), FindLessThanRange(value));
}

// first range which is greater than or equal to value
list<Range>::iterator ObsRange::aboveRange(float64_t value)
{
   return find_if(ranges_.begin(), ranges_.end(), FindLessThanRange(value));
}

float64_t ObsRange::totalRange() const
{
   float64_t total(0);
   for (list<Range>::const_iterator index = ranges_.begin();
	index != ranges_.end(); ++index)
   {
      total += (*index).totalRange();
   }
   return total;
}


// return the sum of all the ranges that are larger than
// the specified minimum range width

float64_t ObsRange::totalRangeGt(float64_t minWidth) const
{
   float64_t total(0);
   for (list<Range>::const_iterator index = ranges_.begin();
	index != ranges_.end(); ++index)
   {
      float64_t rangeWidth = (*index).totalRange();
      if (rangeWidth > minWidth)
      { 
	 total += rangeWidth;
      }
   }
  
   return total;

}


float64_t ObsRange::maxValue() const
{
   float64_t max_(-1.0);
   for (list<Range>::const_iterator index = ranges_.begin();
	index != ranges_.end(); ++index)
   {
      if (max_ < (*index).high_)
      {
	 max_ = (*index).high_;
      }
   }
   return max_;
}


bool ObsRange::hasRangeGt(float64_t minimumBandwidth) const
{
   for (list<Range>::const_iterator index = ranges_.begin();
	index != ranges_.end(); ++index)
   {
      if ((*index).totalRange() > minimumBandwidth)
      {
	 return true;
      }
   }

   return false;
}

void ObsRange::convertFreqBandToObsRange( 
		   const vector<FrequencyBand> & bands)
{
   for (vector<FrequencyBand>::const_iterator index =
	   bands.begin(); index != bands.end(); ++index)
   {
      this->addInOrder(
      		index->centerFreq - index->bandwidth/2.0,
		  index->centerFreq + index->bandwidth/2.0);
   }

}


// subtract obsrange from range
ObsRange operator-(const Range& range, const ObsRange& obsRange)
{
   ObsRange result;
    
   if (obsRange.ranges_.empty())
   {
      result.addInOrder(range.low_,
			range.high_);
      return result;
   }
  
   float64_t currentLow = range.low_;
   list<Range>::const_iterator lowerIndex =
      find_if(obsRange.ranges_.begin(), obsRange.ranges_.end(),
	      FindContainingRange(currentLow));
  
   if (lowerIndex == obsRange.ranges_.end())
   {
      lowerIndex = obsRange.ranges_.begin();
      result.addInOrder(currentLow,
			(*lowerIndex).low_);
   }

   list<Range>::const_iterator upperIndex = lowerIndex;
   for (++upperIndex; (*lowerIndex).high_ < range.high_;
	lowerIndex = upperIndex, ++upperIndex)
   {
      result.addInOrder((*lowerIndex).high_, range.high_);

      if (upperIndex == obsRange.ranges_.end())
      {
	 break;
      }
    
      lowerIndex = upperIndex;
   }
  
   return result;
}

// subtract range from obsRange
ObsRange operator-(const ObsRange& obsRange, const Range& range)
{
   if (obsRange.ranges_.empty())
   {
      return obsRange;
   }
   if (range.high_ < (*obsRange.ranges_.begin()).low_ )
   {
      return obsRange;
   }
  
   ObsRange result;
    
   for (list<Range>::const_iterator index = obsRange.ranges_.begin();
	index != obsRange.ranges_.end();
	++index)
   {
      // no overlap
      if (((*index).low_ > range.high_) || ((*index).high_ < range.low_))
      {
	 result.addInOrder((*index).low_, (*index).high_);
      }
      else 
      {
	 if (range.low_ <= (*index).low_)
	 {
	    if (range.high_ < (*index).high_)
	    {
	       result.addInOrder(range.high_, (*index).high_);	  
	    }
	 }
	 else if (range.high_ > (*index).high_)
	 {
	    if ((*index).low_ < range.low_)
	    {
	       result.addInOrder((*index).low_, range.low_);
	    }
	 }
	 else
	 {
	    if ((*index).low_ < range.low_)
	    {
	       result.addInOrder((*index).low_, range.low_);
	    }

	    if (range.high_ < (*index).high_)
	    {
	       result.addInOrder(range.high_, (*index).high_);
	    }
	 }
      }
   }
  
   return result;
}

// subtract obsRange from obsRange
ObsRange operator-(const ObsRange& left, const ObsRange& right)
{
   ObsRange currentObsRange(left);
   for (list<Range>::const_iterator index = right.ranges_.begin();
	index != right.ranges_.end();
	++index)
   {
      currentObsRange = currentObsRange - *index;
   }

   return currentObsRange;
}


// subtract band from obsRange
ObsRange operator-(const ObsRange& obsRange, 
		   const vector<FrequencyBand> & bands)
{
   ObsRange currentObsRange(obsRange);
   for (vector<FrequencyBand>::const_iterator index =
	   bands.begin(); index != bands.end(); ++index)
   {
      Range range(index->centerFreq - index->bandwidth/2.0,
		  index->centerFreq + index->bandwidth/2.0);
      currentObsRange = currentObsRange - range;
   }

   return currentObsRange;
}

// TODO: consider using Range's operator<< for this
ostream& operator << (ostream& strm, const ObsRange& obsRange)
{
   stringstream localStrm;
   
   localStrm.precision(RangePrintPrecision);
   bool addBlank = false;
   for (list<Range>::const_iterator i = obsRange.ranges_.begin();
	i != obsRange.ranges_.end(); ++i)
   {
      if (addBlank)
      {
	 localStrm << ' ';
      }
      else
      {
	 addBlank = true;
      }
      localStrm << setiosflags(ios::fixed);
      localStrm << (*i).low_ << "-" << (*i).high_;
   }
   
   strm << localStrm.str().c_str();

   return strm;
}

ostream& operator << (ostream& strm, const Range& range)
{
   stringstream localStrm;

   localStrm.precision(RangePrintPrecision);
   localStrm << setiosflags(ios::fixed);
   localStrm << range.low_ << "-" << range.high_;

   strm << localStrm.str().c_str();

   return strm;
}


// remove all the ranges that are less than the given width

void ObsRange::removeRangesLtWidth(float64_t minWidth) 
{
   list<Range>::iterator index = ranges_.begin();
   while (index != ranges_.end())
   {
      float64_t rangeWidth = (*index).totalRange();
      if (rangeWidth < minWidth)
      {
	 index = ranges_.erase(index);
      }	
      else
      {
	 ++index;
      }
   }
}


void ObsRange::removeAllRanges() 
{
   ranges_.clear();
}

float64_t ObsRange::minValue() const
{
   Assert(ranges_.size() > 0);

   return (*ranges_.begin()).low_;
}

list<Range>::const_iterator ObsRange::rangeEnd() const
{
   return ranges_.end();
}

bool ObsRange::isEmpty() const
{
   return ranges_.empty();
}
