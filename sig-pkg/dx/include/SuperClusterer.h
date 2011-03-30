/*******************************************************************************

 File:    SuperClusterer.h
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
// Super-clustering class
//
// $Header: /home/cvs/nss/sonata-pkg/dx/include/SuperClusterer.h,v 1.1 2009/03/06 21:52:00 kes Exp $
//
#ifndef _SuperClustererH
#define _SuperClustererH

#include <vector>
#include <sseDxInterface.h>
#include "SignalIdGenerator.h"
#include "DxTypes.h"
#include "Lock.h"

using std::vector;
using namespace sonata_lib;

namespace dx {

class ChildClusterer;

struct ClusterTag
{
	ChildClusterer *holder;
	int index;
};

class SuperClusterer {
public:
#ifdef notdef
	static SuperClusterer *getInstance();
#endif
	SuperClusterer();
	~SuperClusterer();

	void setObsParams(int32_t activityId, int nSpectra, double baseFreq,
			double chanWidth, double binWidth,
			SignalIdGenerator& sigGen);
	void setSuperClusterGap(float64_t gap);

	// for generating signal ids during secondary confirmation
	SignalId& generateNewSignalId() { return signalIdGen.getNextId(); }

	void attach(ChildClusterer *);
	void detach(ChildClusterer *);

	void clear();
	void compute();

	int getCount();
	bool getNthBadBand(int);
	const ClusterTag &getNthMainSignal(int);
	Polarization getNthPolarization(int);
	SignalId& getNthSignalId(int);
	int getNthClusterCount(int);
	const ClusterTag &getNthMthCluster(int n, int m);

private:
#ifdef notdef
	static SuperClusterer *instance;
#endif

	struct ObsParams {
		int32_t activityId;
		int nSpectra;
		double baseFreq;
		double chanWidth;
		double binWidth;

		ObsParams(): activityId(-1), nSpectra(64), baseFreq(1.0),
				chanWidth(1.0), binWidth(1.0) {}
		ObsParams(int32_t activityId_, int nSpectra_, double baseFreq_,
				double chanWidth_, double binWidth_): activityId(activityId_),
			nSpectra(nSpectra_), baseFreq(baseFreq_), chanWidth(chanWidth_),
			binWidth(binWidth_) {}
	};

	struct SuperClusterDescription
	{
		SignalId signalId;
		SignalType type;				// used in pol type determination
		Polarization pol;
		float64_t hiBound;
		ClusterTag strongest;
		vector<ClusterTag> clusters;
	};

	vector<ChildClusterer *> children;
	vector<SuperClusterDescription> superClusters;
	float64_t superClusterGap;

	ObsParams obsParams;
	SignalIdGenerator signalIdGen;

	Lock sLock;
	void lock() { sLock.lock(); }
	void unlock() { sLock.unlock(); }

	void setTypeAndPol(ClusterTag& tag,SuperClusterDescription *super);
	SignalType getSignalDescription(ClusterTag& tag,
			SignalDescription *sig);

#ifdef notdef
	// hidden
	SuperClusterer();
#endif

	// disallow
	SuperClusterer(const SuperClusterer&);
	SuperClusterer& operator=(const SuperClusterer&);
};

}

#endif
