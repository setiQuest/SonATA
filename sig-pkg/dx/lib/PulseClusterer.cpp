/*******************************************************************************

 File:    PulseClusterer.cpp
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
// PD Clustering object
//
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/PulseClusterer.cpp,v 1.2 2009/05/24 23:42:12 kes Exp $
//
#include <iostream>
#include <stdlib.h>
#include <values.h>
#include <fftw3.h>
#include "DxErr.h"
#include "Log.h"
#include "Partition.h"
#include "PulseClusterer.h"
#include "Statistics.h"
#include "System.h"

using std::cout;
using std::endl;

namespace dx {

#define EDG_MODNAME "Clusterer"
#include "edg_trace.h"

TR_DECLARE(detail);

PulseClusterer::PulseClusterer( SuperClusterer *parent, Resolution res )
	: ChildClusterer( parent )
	, resolution(res)
	, clusterRange(20)
	, bogusTriplets(0)
	//, binWidth(0.6958917)
	//, clusterRange(3)
	//, baseFreq(1.0)
{
	clusterList.reserve(DEFAULT_CLUSTERS);
}

PulseClusterer::~PulseClusterer()
{
	clearHits(); // to free memory pointed to by cluster list
}

//-------------------------
// cluster access
//-------------------------
int
PulseClusterer::getCount()
{
	if (!isComplete())
		Fatal(666);
	return clusterList.size();
}

float64_t
PulseClusterer::getNominalFreq(int idx)
{
	if (!isComplete() || (idx < 0 || idx > (int) clusterList.size()))
		Fatal(666);
	return clusterList[idx]->sig.path.rfFreq;
}

bool
PulseClusterer::isStronger(int idx, ClusterTag *old)
{
	if (!isComplete() || (idx < 0 || idx > (int) clusterList.size()))
		Fatal(666);
	lock();
	PulseClusterer *oldClust = old->holder->getPulse();
	bool stronger = false;
	if (oldClust) {
		const PulseSignalHeader &oldpd = oldClust->getNth(old->index);
		const PulseSignalHeader &newpd = getNth(idx);
		stronger = (newpd.sig.path.power > oldpd.sig.path.power);
	}
	unlock();
	return (stronger);
}

PulseSignalHeader &
PulseClusterer::getNth(int idx)
{
	if (!isComplete())
		Fatal(666);
	if (idx < 0 || idx > (int) clusterList.size()) {
		ErrStr(idx, "index");
		ErrStr(clusterList.size(), "pd clusters");
		ErrStr(parent->getCount(), "supercluster count");
		Fatal(666);
	}
	return *clusterList[idx];
}

PulseClusterer *
PulseClusterer::getPulse()
{
	return this;
}

//-------------------------
// hit recording
//-------------------------
void
PulseClusterer::recordTriplet(
		int spectrum1, int bin1, float32_t power1, Polarization pole1,
		int spectrum2, int bin2, float32_t power2, Polarization pole2,
		int spectrum3, int bin3, float32_t power3, Polarization pole3)
{
	if (power1==0 || power2==0 || power3 == 0) {
		bogusTriplets++;
		return;
	}

//	cout << "record triplet, pol = " << pole << endl;
//	cout << " p0 " << spectrum1 << ", " << bin1 << ", " << power1 << endl;
//	cout << " p1 " << spectrum2 << ", " << bin2 << ", " << power2 << endl;
//	cout << " p2 " << spectrum3 << ", " << bin3 << ", " << power3 << endl;
	Triplet t;
	t.pulses[0].spectrum = spectrum1;
	t.pulses[0].bin = bin1;
	t.pulses[0].power = power1;
	t.pulses[0].pole = pole1;
	t.pulses[1].spectrum = spectrum2;
	t.pulses[1].bin = bin2;
	t.pulses[1].power = power2;
	t.pulses[1].pole = pole2;
	t.pulses[2].spectrum = spectrum3;
	t.pulses[2].bin = bin3;
	t.pulses[2].power = power3;
	t.pulses[2].pole = pole3;

	// find nominal startbin and drift via linear regression
	initLR();
	for (int i=0; i < TSZ; i++)
		addPulseToLR(t.pulses[i]);
	float32_t driftInBinsPerSpectrum;
	if (getLRResult(&t.startBin,&driftInBinsPerSpectrum)) {
		t.drift = driftInBinsPerSpectrum * getSpectraPerObs();
		float midBin = t.startBin + t.drift/2;
		lock();
		tripletList.insert(pair<float,Triplet>(midBin,t));
		unlock();
	}
	else {
		bogusTriplets++;
	}
}

void
PulseClusterer::allHitsLoaded()
{
	if (!isComplete()) {
		// scan multi-map in mid-bin order
		Train cluster;
		bool first = true;
		TripletList::iterator i;
	//	cout << (dec) << tripletList.size() << " triplets" << endl;
		lock();
		for (i = tripletList.begin(); i!= tripletList.end(); i++) {
			bool switchClusters = false;
			if (first)
				switchClusters = true;
			else if (!absorb(cluster, *i))
				switchClusters = true;

			if (switchClusters) {
				if (!first)
					clusterDone(cluster);
				first = false;
				// start new cluster
				cluster.pulses.clear();
				cluster.histogram.clear();
				int period = (*i).second.pulses[1].spectrum
							- (*i).second.pulses[0].spectrum;
				cluster.histogram[period].val += 1;
				for (int j=0; j<TSZ; j++)
					cluster.addPulse((*i).second.pulses[j]);
				cluster.loBin = cluster.hiBin = (*i).first;
			}

		}
		if (!first)
			clusterDone(cluster);

		ChildClusterer::allHitsLoaded();
		unlock();
	}
}

int
PulseClusterer::getTriplets()
{
	return (tripletList.size());
}

void
PulseClusterer::clearHits()
{
	lock();
	ChildClusterer::clearHits();
	bogusTriplets = 0;
	tripletList.clear();
	// free memory pointed to by cluster list
	ClusterList::iterator i;
	for (i = clusterList.begin(); i!= clusterList.end(); i++)
		fftwf_free((*i));
	clusterList.clear();
	unlock();
}

//-------------------------
// private
//-------------------------
void
PulseClusterer::initLR()
{
	sumx = 0;
	sumxx = 0;
	sumy = 0;
	sumxy = 0;
	pulseCnt = 0;
}

void
PulseClusterer::addPulseToLR(const Pulse &pulse)
{
	double spec = pulse.spectrum;
	double bin = pulse.bin;
	Tr(detail,("add LR pulse spec %f, bin %f", spec, bin));
	sumx += spec;
	sumy += bin;
	sumxx += spec*spec;
	sumxy += spec*bin;
	pulseCnt++;
}

bool
PulseClusterer::getLRResult(float32_t *startBinP, float32_t *driftP)
{
	double del = pulseCnt * sumxx - sumx*sumx;
	if (del < 1)
		return false;
	*driftP = pulseCnt*sumxy/del - sumx*sumy/del;
	*startBinP = sumxx*sumy/del - sumx*sumxy/del;
	// XXX range-check startBin and drift; return false if bad ???
	Tr(detail,("LR start bin %f, drift %f", *startBinP, *driftP));
	return true;
}

bool
PulseClusterer::absorb(Train &cluster, const pair<float, Triplet>&i)
{
	if (i.first > cluster.hiBin + clusterRange)
		return false;
	// adjust signal
	Tr(detail,("absorb triplet at %f into train max %f",
			i.first, cluster.hiBin + clusterRange));
	int period = i.second.pulses[1].spectrum
				- i.second.pulses[0].spectrum;
	cluster.histogram[period].val += 1;
	cluster.hiBin = i.first;
	for (int j=0; j<TSZ; j++)
		cluster.addPulse(i.second.pulses[j]);
	return true;
}

void
PulseClusterer::Train::addPulse(const Pulse &pulse)
{
	// The following line builds a map entry if one doesn't exist,
	// but the default ctor for the Pulse class sets the power to
	// zero.  We check for that below.
	map<int, Pulse>::iterator p = pulses.find(pulse.spectrum);
	if (p != pulses.end()) {
		// a pulse already exists for this spectrum;
		// only overwrite if new pulse has more power
		if (pulse.power > p->second.power)
			p->second = pulse;
	}
	else if (pulses.size() < MAX_TRAIN_PULSES) {
		pulses.insert(std::make_pair(pulse.spectrum, pulse));
	}
//	cout << "inmap, pol " << inMap->pole << ", s " << inMap->spectrum;
//	cout << ", b " << inMap->bin << ", p " << inMap->power << endl;
}

void
PulseClusterer::clusterDone(Train &cluster)
{
	// allocate enough memory to hold the header and the
	size_t len = sizeof(PulseSignalHeader)
			+ cluster.pulses.size() * sizeof(::Pulse);
	PulseSignalHeader *hdr =
			static_cast<PulseSignalHeader *> (fftwf_malloc(len));
	Assert(hdr);

	// do 2nd linear regression for start bin and drift
	initLR();
	map<int,Pulse>::iterator i;
//	cout << "train begin" << endl;
	for (i = cluster.pulses.begin(); i!= cluster.pulses.end(); i++) {
		Pulse pulse = (*i).second;
//		cout << "p = " << pulse.pole << ", s = " << (dec) << pulse.spectrum;
//		cout << ", b = " << (dec) << pulse.bin << ", p = " << pulse.power;
//		cout << endl;
		addPulseToLR((*i).second);
	}
	float32_t driftInBinsPerSpectrum = 0;
	float32_t startBin = 0;
	if (!getLRResult(&startBin,&driftInBinsPerSpectrum))
		Fatal(666);

	// format cluster description
	hdr->sig.path.rfFreq = binsToAbsoluteMHz((int) startBin);

#ifdef notdef
	// if the clustered signal lies outside the channel, discard it with
	// a warning to the SSE
	if (hdr->sig.path.rfFreq < baseFreq || hdr->sig.path.rfFreq >= highFreq) {
		LogWarning(ERR_SOC, activityId, "freq = %lf, low = %lf, high = %lf",
				hdr->sig.path.rfFreq, baseFreq, highFreq);
		fftwf_free(hdr);
		return;
	}
#endif
#ifdef notdef
	hdr->sig.signalId = 0; // XXX
#endif
	hdr->sig.path.drift = binsToRelativeHz((int) (driftInBinsPerSpectrum
									  * getSpectraPerObs()))/getSecondsPerObs();
	hdr->sig.path.width
			= binsToRelativeHz((int) (1 + cluster.hiBin - cluster.loBin));
//	cout << "f " << hdr->sig.path.rfFreq << ", d " << hdr->sig.drift;
//	cout << ", w " << hdr->sig.path.width << endl;
	int32_t n = 0;
	for (i = cluster.pulses.begin(); i!= cluster.pulses.end(); ++i)
	{
		if (i == cluster.pulses.begin()) {
			hdr->sig.path.power = (*i).second.power;
			hdr->sig.pol = (*i).second.pole;
		}
		else {
			hdr->sig.path.power += (*i).second.power;
			if (hdr->sig.pol != (*i).second.pole)
				hdr->sig.pol = POL_MIXED;
		}
		// dual-pol signals count as two bins
		switch ((*i).second.pole) {
		case POL_BOTH:
			++n;
		case POL_LEFTCIRCULAR:
		case POL_RIGHTCIRCULAR:
		default:
			++n;
			break;
		}
//		cout << "pulse (" << (*i).second.spectrum << ", " << (*i).second.bin;
//		cout << ", " << (*i).second.power << ")" << endl;
	}

//	cout << "total power " << hdr->sig.power << endl;
//	cout << "pulse count " << n << endl;
//	cout << "bin width " << binsToRelativeHz(1) << endl;
	// compute the SNR and PFA for the signal
	hdr->cfm.snr = ((hdr->sig.path.power - n) / n) * binsToRelativeHz(1);
	hdr->cfm.pfa = computePfa(n, hdr->sig.path.power);

	hdr->sig.sigClass = CLASS_UNINIT; // XXX
	hdr->sig.reason = CLASS_REASON_UNINIT; // XXX
	Tr(detail,(
		"cluster @%f drift %f lo %f hi %f",
		hdr->sig.path.rfFreq, hdr->path.ig.drift, cluster.loBin,
		cluster.hiBin));

	map<int,Counter>::iterator j;
	int hiCount = 0;
	int periodInSpectra = 0;
	for (j = cluster.histogram.begin(); j!= cluster.histogram.end(); j++) {
		if ((*j).second.val > hiCount) {
			hiCount = (*j).second.val;
			periodInSpectra = (*j).first;
		}
	}
	hdr->train.pulsePeriod = (periodInSpectra * getSecondsPerObs())
							/ getSpectraPerObs();
	hdr->train.numberOfPulses = cluster.pulses.size();
	hdr->train.res = resolution;

	// array of individual pulses
	::Pulse *p = (::Pulse *)(hdr + 1);
//	cout << "PulseClusterer, record pulses" << endl;
	for (i = cluster.pulses.begin(); i!= cluster.pulses.end(); i++) {
		p->rfFreq = binsToAbsoluteMHz((*i).second.bin);
		p->power = (*i).second.power;
		p->spectrumNumber = (*i).second.spectrum;
		p->binNumber = (*i).second.bin;
		p->pol = (*i).second.pole;
//		cout << "pulse " << *p;
		p++;
	}

	clusterList.push_back(hdr);
}

float32_t
PulseClusterer::computePfa(int32_t n, float32_t power)
{
	float64_t pfa, logPfa, pPulse;

	// pulse probability is doubled because there are two polarizations
	pPulse = 2 * pow(M_E, -pulseThreshold);
	pfa = (bins * pow(4.0, n - 2) * pow(spectra, 3) * pow(pPulse, n)) / 3.0;
	if (pfa < DBL_MIN)
		logPfa = -FLT_MAX;
	else
		logPfa = log(pfa);

//	cout << "bins = " << bins << ", sp = " << spectra << ", pulse prob = " << pPulse << endl;
//	cout << "n = " << n << ", power = " << power;
//	cout << ", thresh " << pulseThreshold;
//	cout << ", base pfa = " << logPfa;
	if ((power -= n * pulseThreshold) < 0)
		power = 0;
	logPfa += ChiSquare(2 * n, 2 * power);
	if (logPfa < -FLT_MAX)
		logPfa = -FLT_MAX;
//	cout << ", log pfa = " << logPfa << endl;
	return (logPfa);
}

}
