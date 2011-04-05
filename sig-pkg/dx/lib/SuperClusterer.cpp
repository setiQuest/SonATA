/*******************************************************************************

 File:    SuperClusterer.cpp
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
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/SuperClusterer.cpp,v 1.2 2009/05/24 23:45:36 kes Exp $
//

#include <iostream>
#include "SuperClusterer.h"
#include "ChildClusterer.h"
#include "CwClusterer.h"
#include "PulseClusterer.h"
#include "SignalIdGenerator.h"
#include "Err.h"

#define EDG_MODNAME "Clusterer"
#include "edg_trace.h"

using namespace sonata_lib;
using std::cout;
using std::endl;

namespace dx {

TR_DECLARE(detail);

#ifdef notdef
SuperClusterer *SuperClusterer::instance = 0;

SuperClusterer *
SuperClusterer::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new SuperClusterer();
	l.unlock();
	return (instance);
}
#endif

SuperClusterer::SuperClusterer()
	: superClusterGap(.001)
{
}

SuperClusterer::~SuperClusterer()
{
	vector<ChildClusterer *>::iterator p, q;
	lock();
	while ((p = children.begin()) != children.end())
		delete (*p);
	unlock();
}

void
SuperClusterer::setObsParams(int32_t actId, int nSpectra, double baseFreq,
		double chanWidth, double binWidth, SignalIdGenerator& sigGen)
{
	lock();
	signalIdGen = sigGen;
	obsParams = ObsParams(actId, nSpectra, baseFreq, chanWidth, binWidth);
	for (int i = 0; i < (int) children.size(); ++i) {
		ChildClusterer *ch = children[i];
		ch->setObsParams(actId, nSpectra, baseFreq, chanWidth, binWidth);
	}
	unlock();
}

void
SuperClusterer::setSuperClusterGap(float64_t gap)
{
	superClusterGap = gap;
}

void
SuperClusterer::clear()
{
	lock();
	superClusters.clear();
	for (int i = 0; i < (int) children.size(); ++i) {
		ChildClusterer *ch = children[i];
		ch->clearHits();
	}
	unlock();
}

void
SuperClusterer::compute()
{
	vector<int> index;
	vector<float64_t> freq;

	if (!children.size())
		return;

	// initialize merge cursors for each cluster list
	lock();
	for (int i = 0; i < (int) children.size(); ++i) {
		ChildClusterer *ch = children[i];
		index.push_back(0);
		freq.push_back(ch->getCount() ? ch->getNominalFreq(0) : 0);
	}
	unlock();

	SuperClusterDescription *lastSuper = 0;

	// until everything is merged
	Tr(detail,("begin computing superclusters"));
	lock();
	while (true) {
		// find the lowest next frequency
		int nextList = -1;
		for (int i = 0; i < (int) children.size(); ++i) {
			if (freq[i] && (nextList < 0 || freq[i] < freq[nextList])) {
				// still clusters in this list
				nextList = i;
			}
		}
		if (nextList < 0)
			break; // no more clusters in any list

		if (lastSuper && freq[nextList] < lastSuper->hiBound) {
			// merge this cluster into super cluster
			lastSuper->hiBound = freq[nextList] + superClusterGap;
			ClusterTag tag;
			tag.holder = children[nextList];
			tag.index = index[nextList];
			lastSuper->clusters.push_back(tag);
			if (tag.holder->isStronger(tag.index, &lastSuper->strongest))
				lastSuper->strongest = tag;
			// add this signal polarization to the supercluster
			setTypeAndPol(tag, lastSuper);
			Tr(detail,("merge cluster from list %d @%f, hiBound %f",
					nextList, freq[nextList], lastSuper->hiBound));
		}
		else {
			// create new super cluster
			SuperClusterDescription desc;
			desc.signalId = signalIdGen.getNextId();
			desc.hiBound = freq[nextList] + superClusterGap;
			ClusterTag tag;
			tag.holder = children[nextList];
			tag.index = index[nextList];
			desc.strongest = tag;
			desc.clusters.push_back(desc.strongest);
			CwClusterer *cwCluster;
			PulseClusterer *pulseCluster;
			// set the initial polarization based on the first cluster
			cwCluster = tag.holder->getCw();
			pulseCluster = tag.holder->getPulse();
			if (cwCluster) {
				CwPowerSignal cwSig = cwCluster->getNth(tag.index);
				desc.type = CW_POWER;
				desc.pol = cwSig.sig.pol;
			}
			else if (pulseCluster) {
				PulseSignalHeader pulseSig = pulseCluster->getNth(tag.index);
				desc.type = PULSE;
				desc.pol = pulseSig.sig.pol;
			}
			superClusters.push_back(desc);
			lastSuper = &superClusters[superClusters.size() - 1];
			Tr(detail,("create new supercluster from list %d @%f, hiBound %f",
					nextList, freq[nextList], desc.hiBound));
		}
		// advance cursor for the cluster list we just merged from
		ChildClusterer *ch = children[nextList];
		if (++index[nextList] >= ch->getCount())
			freq[nextList] = 0;
		else
			freq[nextList] = ch->getNominalFreq(index[nextList]);
	}
	unlock();
	Tr(detail,("done computing %d supercluster(s)",superClusters.size()));
}

int
SuperClusterer::getCount()
{
	return superClusters.size();
}

const ClusterTag &
SuperClusterer::getNthMainSignal(int idx)
{
	if (idx < 0 || idx >= (int) superClusters.size())
		Fatal(666);
	return superClusters[idx].strongest;
}

Polarization
SuperClusterer::getNthPolarization(int idx)
{
	if (idx < 0 || idx >= (int) superClusters.size())
		Fatal(666);
	return superClusters[idx].pol;
}

SignalId&
SuperClusterer::getNthSignalId(int idx)
{
	if (idx < 0 || idx >= (int) superClusters.size())
		Fatal(666);
	return superClusters[idx].signalId;
}

int
SuperClusterer::getNthClusterCount(int idx)
{
	if (idx < 0 || idx >= (int) superClusters.size())
		Fatal(666);
	return superClusters[idx].clusters.size();
}

const ClusterTag &
SuperClusterer::getNthMthCluster(int idx, int idx2)
{
	if (idx < 0 || idx >= (int) superClusters.size())
		Fatal(666);
	if (idx2 < 0 || idx2 > (int) superClusters[idx].clusters.size())
		Fatal(666);
	return superClusters[idx].clusters[idx2];
}

void
SuperClusterer::attach(ChildClusterer *child)
{
	lock();
	child->setObsParams(obsParams.activityId, obsParams.nSpectra,
			obsParams.baseFreq, obsParams.chanWidth, obsParams.binWidth);
	children.push_back(child);
	unlock();
}

void
SuperClusterer::detach(ChildClusterer *child) {
	vector<ChildClusterer *>::iterator p;
	lock();
	for (p = children.begin(); p != children.end(); ++p) {
		if (*p == child) {
			children.erase(p);
			break;
		}
	}
	unlock();
}

//
// setTypeAndPol: set the type and polarization of the supercluster
//
// Notes:
//		Any supercluster which contains a CW signal becomes a
//		CW supercluster; i.e.,  a supercluster will be a Pulse only
//		if it contains no CW signals.
//		A CW supercluster is POL_BOTH if it contains any signals
//		(either CW or pulse) of both types of polarization.
//		A pulse supercluster is POL_BOTH only if it consists
//		entirely of pulses which are POL_BOTH.  If it contains
//		any pulses of different polarization, it will be
//		defined as POL_MIXED.
//
void
SuperClusterer::setTypeAndPol(ClusterTag& tag, SuperClusterDescription *super)
{
	SignalType type;
	SignalDescription sig;

	type = getSignalDescription(tag, &sig);
	switch (type) {
	case CW_POWER:
		// force type to CW
		super->type = CW_POWER;
		switch (super->pol) {
		case POL_RIGHTCIRCULAR:
		case POL_LEFTCIRCULAR:
			if (sig.pol != super->pol)
				super->pol = POL_BOTH;
			break;
		case POL_MIXED:
			super->pol = POL_BOTH;
			break;
		default:
			break;
		}
		break;
	case PULSE:
		switch (super->type) {
		case CW_POWER:
			if (sig.pol != super->pol)
				super->pol = POL_BOTH;
			break;
		case PULSE:
			if (sig.pol != super->pol)
				super->pol = POL_MIXED;
			break;
		default:
			Fatal(666);
		}
		break;
	default:
		Fatal(666);
		break;
	}
}

SignalType
SuperClusterer::getSignalDescription(ClusterTag& tag, SignalDescription *sig)
{
	SignalType type = ANY_TYPE;

	if (CwClusterer *cwCluster = tag.holder->getCw()) {
		CwPowerSignal cwSig = cwCluster->getNth(tag.index);
		*sig = cwSig.sig;
		type = CW_POWER;
	}
	else if (PulseClusterer *pulseCluster = tag.holder->getPulse()) {
		PulseSignalHeader pulseSig = pulseCluster->getNth(tag.index);
		*sig = pulseSig.sig;
		type = PULSE;
	}
	else
		Fatal(666);
	return (type);
}

}
