/*******************************************************************************

 File:    Range.h
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

/*****************************************************************
 * Range.h - declaration of functions defined in Range.h
 * Classes which store ranges of numbers
 * PURPOSE:  
 *****************************************************************/

#ifndef RANGE_H
#define RANGE_H

#include <iostream>
#include <list>
#include <vector>
#include "machine-dependent.h" // for float64_t et. al.

using std::list;
using std::vector;
using std::ostream;

class FrequencyBand;

class Range 
{
public:
  Range(float64_t low, float64_t high);
  virtual float64_t totalRange() const; // total frequency range in Range
  virtual ~Range();

  // default copy constructor and assignment operator are OK

  float64_t low_;
  float64_t high_;
};

ostream& operator << (ostream& strm, const Range& range);

class ObsRange 
{
public:
  ObsRange();
  virtual ~ObsRange();

  // default copy constructor and assignment operator are OK

  virtual bool isEmpty() const;
  virtual void addInOrder(float64_t low, float64_t high);
  virtual void addOutOfOrder(float64_t low, float64_t high);
  virtual bool isIncluded(float64_t value) const;
  virtual bool isIncluded(const Range& range) const;
  virtual float64_t getUseableBandwidth(const Range& range) const;
  virtual bool hasRangeGt(float64_t minimumBandwith) const;
  virtual list<Range>::const_iterator aboveRange(float64_t value) const;
  virtual list<Range>::iterator aboveRange(float64_t value);
  virtual list<Range>::const_iterator rangeEnd() const;
  virtual void convertFreqBandToObsRange(
			const vector<FrequencyBand> &bands);

  // total frequency range in Range
  virtual float64_t totalRange() const;
  virtual float64_t totalRangeGt(float64_t minWidth) const;

  virtual float64_t minValue() const;
  virtual float64_t maxValue() const;

  virtual void removeRangesLtWidth(float64_t minWidth);
  virtual void removeAllRanges();
  friend ostream& operator << (ostream& strm, const ObsRange& obsRange);
  friend ObsRange operator-(const Range& range, const ObsRange& obsRange);
  friend ObsRange operator-(const ObsRange& obsRange, const Range& range);
  friend ObsRange operator-(const ObsRange& obsRange,
			    const vector<FrequencyBand> & bands);
  friend ObsRange operator-(const ObsRange& left, const ObsRange& right);

protected:

  // merges Ranges together, if possible
  virtual void merge();

private:

  list<Range> ranges_;

};

#endif /* RANGE_H */
