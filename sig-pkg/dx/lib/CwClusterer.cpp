/*******************************************************************************

 File:    CwClusterer.cpp
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
// CWD Clustering object
//
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/CwClusterer.cpp,v 1.2 2009/05/24 23:41:29 kes Exp $
//
#include <iostream>
#include "CwClusterer.h"
#include "DxErr.h"
#include "SuperClusterer.h"
#include "System.h"

using std::cout;
using std::endl;

namespace dx {

#define EDG_MODNAME "Clusterer"
#include "edg_trace.h"

TR_DECLARE(detail);

CwClusterer::CwClusterer( SuperClusterer *parent, Polarization p  )
	: ChildClusterer( parent )
	, clusterRange(3)
	, pole(p)
{
	clusterList.reserve(DEFAULT_CLUSTERS);
}

CwClusterer::~CwClusterer()
{
	clearHits();
}

//-------------------------
// clustering control
//-------------------------
void
CwClusterer::setClusterRange(int32_t bins)
{
	clusterRange = bins;
}

//-------------------------
// cluster access
//-------------------------
int
CwClusterer::getCount()
{
	if (!isComplete())
		Fatal(666);
	return clusterList.size();
}

float64_t
CwClusterer::getNominalFreq(int idx)
{
	if (!isComplete() || (idx < 0 || idx >= (int) clusterList.size()))
		Fatal(666);
	return clusterList[idx].sig.path.rfFreq;
}

bool
CwClusterer::isStronger(int idx, ClusterTag *old)
{
	if (!isComplete() || (idx < 0 || idx >= (int) clusterList.size()))
		Fatal(666);
	lock();
	CwClusterer *oldClust = old->holder->getCw();
	bool stronger = true;
	if (oldClust) {
		const CwPowerSignal &oldcw = oldClust->getNth(old->index);
		const CwPowerSignal &newcw = getNth(idx);
		stronger = (newcw.sig.path.power > oldcw.sig.path.power);
	}
	unlock();
	return (stronger);
}

CwPowerSignal &
CwClusterer::getNth(int idx)
{
	if (!isComplete())
		Fatal(666);
	if (idx < 0 || idx >= (int) clusterList.size()) {
		ErrStr(idx, "index");
		ErrStr(clusterList.size(), "pd clusters");
		ErrStr(parent->getCount(), "supercluster count");
		Fatal(666);
	}
	return clusterList[idx];
}

CwClusterer *
CwClusterer::getCw()
{
    return this;
}

//-------------------------
// hit recording
//-------------------------
void
CwClusterer::recordHit(int startBin, int drift, int power)
{
	if (isComplete())
		Fatal(666);
	// compute mid-bin
	float midBin = startBin + (drift + 0.5)/2;
	// insert into multi-map indexed by mid-bin
	HitReport report(startBin, drift, power);
	lock();
	hitList.insert(pair<float,HitReport>(midBin,report));
	unlock();
}

void
CwClusterer::allHitsLoaded()
{
	if (!isComplete()) {
		// scan multi-map in mid-bin order
		Cluster cluster;
		bool first = true;
		HitList::iterator i;
		lock();
		for (i = hitList.begin(); i!= hitList.end(); i++)
		{
			bool switchClusters = false;
			if (first)
				switchClusters = true;
			else if (!absorb(cluster, *i))
				switchClusters = true;

			if (switchClusters)
			{
				if (!first)
					clusterDone(cluster);
				first = false;
				// start new cluster
				cluster.hr = (*i).second;
				cluster.loBin = cluster.hiBin = (*i).first;
				Tr(detail,(
					"new cluster start %d drift %d power %d lo %f hi %f bb %d",
					cluster.hr.startBin, cluster.hr.drift, cluster.hr.power,
					cluster.loBin, cluster.hiBin));
			}
		}
		if (!first)
			clusterDone(cluster);

		ChildClusterer::allHitsLoaded();
		unlock();
	}
}

int
CwClusterer::getHits()
{
	return (hitList.size());
}

void
CwClusterer::clearHits()
{
	lock();
	ChildClusterer::clearHits();
	hitList.clear();
	clusterList.clear();
	unlock();
}

//----------------------
// private
//----------------------
bool
CwClusterer::absorb(Cluster &cluster, const pair<float, HitReport>&i)
{
	if (i.first > cluster.hiBin + clusterRange)
		return false;
	// adjust signal
	Tr(detail,("absorb hit at %f into cluster max %f",
				i.first, cluster.hiBin+clusterRange));
	cluster.hiBin = i.first;
	if (i.second.power > cluster.hr.power)
	{
		cluster.hr = i.second;
		Tr(detail,("adjust cluster start %d drift %d power %d lo %f hi %f",
					cluster.hr.startBin, cluster.hr.drift, cluster.hr.power,
					cluster.loBin, cluster.hiBin));
	}
	return true;
}

void
CwClusterer::clusterDone(Cluster &cluster)
{
	CwPowerSignal cw;
	cw.sig.path.rfFreq = binsToAbsoluteMHz(cluster.hr.startBin);
#ifdef notdef
	cw.sig.signalId = 0; // XXX
#endif
	cw.sig.path.drift
			= binsToRelativeHz((int) cluster.hr.drift)/getSecondsPerObs();
	cw.sig.path.width
			 = binsToRelativeHz((int) (1 + cluster.hiBin - cluster.loBin));
	cw.sig.path.power = cluster.hr.power;
	cw.sig.pol = pole;
	cw.sig.sigClass = CLASS_UNINIT; // XXX
	cw.sig.reason = CLASS_REASON_UNINIT; // XXX
	clusterList.push_back(cw);
}

}
