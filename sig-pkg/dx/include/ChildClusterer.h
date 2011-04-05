/*******************************************************************************

 File:    ChildClusterer.h
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

//
// Clustering base class
//
// $Header: /home/cvs/nss/sonata-pkg/dx/include/ChildClusterer.h,v 1.1 2009/03/06 21:52:00 kes Exp $
//
#ifndef _ChildClustererH
#define _ChildClustererH

#include <sseDxInterface.h>
#include "Lock.h"
#include "SuperClusterer.h"

using namespace sonata_lib;

namespace dx {
	
class ClusterTag;
class SuperClusterer;
class CwClusterer;
class PulseClusterer;

class ChildClusterer {
public:
	// Create a Child clustering object
	ChildClusterer(SuperClusterer *parent);
	virtual ~ChildClusterer();

	void setObsParams(int32_t actId, int nSpectra, double baseFreq,
			double chanWidth, double binWidth);

	// state management
	virtual void allHitsLoaded();
	virtual void clearHits();
	virtual bool isComplete() { return complete; }

	// interface for super-clustering
	virtual int getCount() = 0;
	virtual float64_t getNominalFreq(int) = 0;
	virtual bool isStronger(int myIndex, ClusterTag *) = 0;

	// upcasting support
	virtual CwClusterer *getCw();
	virtual PulseClusterer *getPulse();

	float64_t binsToRelativeHz(int bins)
	{
		return bins * binWidth;
	}
	
	float64_t binsToAbsoluteMHz(int bins)
	{
		// XXX the 0.5 is here on the assumption that baseFreq is the
		// XXX frequency of the low edge of bin 0, rather than its 
		// XXX center frequency.  Verify this.
#ifdef notdef
		return baseFreq + (binsToRelativeHz(bins) + 0.5 * binWidth) / 1e6;
#else
		return baseFreq + binsToRelativeHz(bins)  / 1e6;
#endif
	}

	void setBinWidth(double binWidth_) { binWidth = binWidth_; }
	void setSpectraPerObs(int spectra_) { spectraPerObs = spectra_; }
	int getSpectraPerObs() { return spectraPerObs; }
	float32_t getSecondsPerObs() { return secondsPerObs; }
protected:
	SuperClusterer *parent;
	Lock cLock;

	void lock() { cLock.lock(); }
	void unlock() { cLock.unlock(); }
protected:
	int32_t activityId;
	int spectraPerObs;
	double baseFreq; // MHz
	double highFreq; // MHz
	double binWidth; // Hz
	double secondsPerObs;

private:
	bool complete;

	// disallow
	ChildClusterer(const ChildClusterer&);
	ChildClusterer& operator=(const ChildClusterer&);
};

}

#endif
