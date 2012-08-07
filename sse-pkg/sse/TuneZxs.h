/*******************************************************************************

 File:    TuneZxs.h
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
 * TuneZxs.h - declaration of functions defined in TuneZxs.h
 * tunes zxs uses preferred strategy
 * PURPOSE:  
 *****************************************************************/

#ifndef TUNEZXS_H
#define TUNEZXS_H

#include "machine-dependent.h"
#include "Range.h"
#include "DxList.h"
#include <mysql.h>

class DxProxy;

class TuneZxs 
{
public:
  TuneZxs(int verboseLevel);
  virtual ~TuneZxs();
  virtual void tune(DxList &zxList, int32_t totalChannels,
		float64_t mhzPerChannel, float64_t chanTuneFreqMhz) = 0;
  virtual bool moreActivitiesToRun() const = 0;
  virtual int getVerboseLevel() const;
  virtual int setVerboseLevel(int level);

  static void tuneZxsFromPrevActInDatabase(
      int oldActivityId, MYSQL *db, DxList &zxList);

protected:

  static void markZxAsNotInUse(DxProxy *zxProxy);

  int verboseLevel_;
};

// uses manual tuning
class TuneZxsUser : public TuneZxs
{
public:
  TuneZxsUser(int verboseLevel);
  virtual ~TuneZxsUser();
  virtual void tune(DxList &zxList, int32_t totalChannels,
		float64_t mhzPerChannel, float64_t chanTuneFreqMhz);
  virtual bool moreActivitiesToRun() const;
};

class TuneZxsAuto : public TuneZxs
{
public:
  TuneZxsAuto(int verboseLevel);
  virtual ~TuneZxsAuto();
protected:
};


// tunes zxs from start to end
class TuneZxsRange : public TuneZxsAuto
{
public:
  TuneZxsRange(int verboseLevel,
		const Range& range);
  virtual ~TuneZxsRange();
  virtual void tune(DxList &zxList, int32_t totalChannels,
		float64_t mhzPerChannel, float64_t chanTuneFreqMhz);
  virtual bool moreActivitiesToRun() const;

protected:
  virtual float64_t calculateCenterFreq(DxProxy *zxProxy);
  const Range range_;
  float64_t nextFreq_;
};

// uses default zx values, repeated forever
class TuneZxsForever : public TuneZxs
{
public:
  TuneZxsForever(int verboseLevel);
  virtual ~TuneZxsForever();
  virtual void tune(DxList &zxList, int32_t totalChannels,
		float64_t mhzPerChannel, float64_t chanTuneFreqMhz);

  virtual bool moreActivitiesToRun() const;
};

class TuneZxsObsRange : public TuneZxsAuto
{
public:
  TuneZxsObsRange(int verboseLevel,
		   const ObsRange& obsRange);
  virtual ~TuneZxsObsRange();

  virtual void tune(DxList &zxList, int32_t totalChannels,
		float64_t mhzPerChannel, float64_t chanTuneFreqMhz);
  virtual bool moreActivitiesToRun() const;

protected:
  virtual float64_t calculateCenterFreq(DxProxy *zxProxy);

  const ObsRange obsRange_;
  float64_t nextLeftEdgeFreq_;

};


#endif /* TUNEZXS_H */
