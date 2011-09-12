/*******************************************************************************

 File:    SignalClassifier.cpp
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
// Signal classifier
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/SignalClassifier.cpp,v 1.4 2009/05/24 23:01:27 kes Exp $
//

#include <iostream>
#include "CwClusterer.h"
#include "CwSignal.h"
#include "DxOpsBitset.h"
#include "PulseClusterer.h"
#include "SignalClassifier.h"

using std::cout;
using std::endl;

namespace dx {

SignalClassifier::SignalClassifier()
{
}

SignalClassifier::~SignalClassifier()
{
}

void
SignalClassifier::clear()
{
}

//
// selectCandidates: build a list of candidates
//
// Notes:
//		For primary detection.
void
SignalClassifier::classifySignals(Activity *act)
{
	SystemType origin = act->getMode();

	Debug(DEBUG_NEVER, 1, "classify signals");

	params = act->getActivityParams();
	int32_t maxCandidates = params.maxNumberOfCandidates;
	recentRfi = act->getRecentRfiMask();
	testSignal = act->getTestSignalMask();
	operations = params.operations;

	act->setCandidatesOverMax(candidatesOverMax = 0);

	SuperClusterer *superClusterer = act->getSuperClusterer();
	Assert(superClusterer);
	// scan the list of signals
	for (int32_t i = 0; i < superClusterer->getCount(); i++) {
		// get the next signal
		ClusterTag tag = superClusterer->getNthMainSignal(i);
		CwClusterer *cwClusterer = tag.holder->getCw();
		PulseClusterer *pulseClusterer = tag.holder->getPulse();
		if (cwClusterer) {
			Debug(DEBUG_NEVER, i, "cw clusterer");
			CwPowerSignal& signal = cwClusterer->getNth(tag.index);
			classifySignal(act, signal.sig, superClusterer, i, maxCandidates);
			signal.sig.containsBadBands = applyBadBands(act, signal.sig);
			if (signal.sig.sigClass == CLASS_CAND)
				act->addCwCandidate(&signal, origin);
			act->addCwSignal(&signal);
		}
		else if (pulseClusterer) {
			Debug(DEBUG_NEVER, i, "pulse cluster");
			PulseSignalHeader *signal = &pulseClusterer->getNth(tag.index);
//			cout << "SignalClassifier pulse " << *signal;
//			Pulse *p = (Pulse *) (signal + 1);
//			for (int j = 0; j < signal->train.numberOfPulses; ++j)
//				cout << "pulse " << j << p[j];
			classifySignal(act, signal->sig, superClusterer, i, maxCandidates);
			signal->sig.containsBadBands = applyBadBands(act, signal->sig);
			if (signal->sig.sigClass == CLASS_CAND)
				act->addPulseCandidate(signal, origin);
			act->addPulseSignal(signal);
//			displayPulseSignal(*signal);
		}
	}
	act->setCandidatesOverMax(candidatesOverMax);
}

void
SignalClassifier::classifySignal(Activity *act, SignalDescription& sig,
		SuperClusterer *superClusterer, int32_t sigNum, int32_t maxCandidates)
{
	sig.pol = superClusterer->getNthPolarization(sigNum);
	sig.subchannelNumber = act->getChannel()->getSubchannel(sig.path.rfFreq);
	sig.signalId = superClusterer->getNthSignalId(sigNum);
	sig.origSignalId = SignalId();

	// initial classification of the signal depends on the
	// operating mode
	sig.sigClass = CLASS_CAND;
	sig.reason = PASSED_POWER_THRESH;

	// test for zero-drift signal
	if (operations.test(REJECT_ZERO_DRIFT_SIGNALS)) {
		float64_t absDrift = fabs(sig.path.drift);
		if (absDrift <= params.zeroDriftTolerance) {
			sig.sigClass = CLASS_RFI;
			sig.reason = ZERO_DRIFT;
		}
	}
	// test for too-big-drift signal
	if (operations.test(REJECT_ZERO_DRIFT_SIGNALS)) {
		float32_t absDrift = fabs(sig.path.drift);
		float32_t maxDrift = params.maxDriftRateTolerance
				* MHZ_TO_GHZ(params.dxSkyFreq);
		if (absDrift > maxDrift) {
				sig.sigClass = CLASS_RFI;
				sig.reason = DRIFT_TOO_HIGH;
		}
	}
	// test for recent RFI mask match
	Debug(DEBUG_SIGNAL_CLASS,
			(int32_t) operations.test(APPLY_RECENT_RFI_MASK),
			"apply recent RFI mask");
	Debug(DEBUG_SIGNAL_CLASS, (void *) recentRfi, "recent RFI mask");
	if (act->getMode() == PRIMARY) {
		if (operations.test(APPLY_RECENT_RFI_MASK) && recentRfi) {
			Debug(DEBUG_SIGNAL_CLASS, (void *) recentRfi, "apply mask?");
			if (recentRfi->isMasked(sig.path.rfFreq,
					HZ_TO_MHZ(sig.path.width))) {
				sig.sigClass = CLASS_RFI;
				sig.reason = RECENT_RFI_MATCH;
				Debug(DEBUG_SIGNAL_CLASS, (void *) recentRfi, "yep");
			} else
				Debug(DEBUG_SIGNAL_CLASS, (void *) recentRfi, "nope");
		}
	}

	// test for test signal mask match; this overrides rfi mask
	if (operations.test(APPLY_TEST_SIGNAL_MASK) && testSignal) {
		if (testSignal->isMasked(sig.path.rfFreq, HZ_TO_MHZ(sig.path.width))) {
			sig.sigClass = CLASS_CAND;
			sig.reason = TEST_SIGNAL_MATCH;
		}
	}
	// test for followup candidates; the signal must already
	// be a candidate;
	if (operations.test(FOLLOW_UP_CANDIDATES)) {
		Signal *signal = findFollowupSignal(act, sig);
		if (signal) {
			sig.origSignalId = signal->getOrigSignalId();
			if (sig.sigClass == CLASS_CAND)
				act->removeFollowupSignal(signal);
			else {
				// preserve the class/reason information for the
				// candidate
				signal->setClass(sig.sigClass);
				signal->setReason(sig.reason);
			}
		}
		else {
			sig.sigClass = CLASS_UNKNOWN;
			sig.reason = PASSED_POWER_THRESH;
		}
	}
	// if we are not selecting candidates, or we have exceeded
	// the maximum number of candidates, mark any candidate signal as
	// class unknown
	if (!operations.test(CANDIDATE_SELECTION))
		sig.sigClass = CLASS_UNKNOWN;
	else if (sig.path.rfFreq < act->getChannel()->getLowFreq()
			|| sig.path.rfFreq >= act->getChannel()->getHighFreq()) {
		sig.sigClass = CLASS_UNKNOWN;
		sig.reason = SIGNAL_NOT_IN_CHANNEL;
	}
	else if (sig.sigClass == CLASS_CAND
			&& act->getCandidateCount(ANY_TYPE) >= maxCandidates) {
		++candidatesOverMax;
		sig.sigClass = CLASS_UNKNOWN;
		sig.reason = TOO_MANY_CANDIDATES;
	}
	Debug(DEBUG_SIGNAL_CLASS, sig.signalId.number, "sig.signalId.number");
	Debug(DEBUG_SIGNAL_CLASS, sig.sigClass, "sig.sigClass");
	Debug(DEBUG_SIGNAL_CLASS, sig.reason, "sig.reason");
}

int32_t
SignalClassifier::getCount()
{
	return (0);
}

int32_t
SignalClassifier::getCandidate(int32_t idx)
{
	return (0);
}

//
// followupSignal: determine whether the specified signal is a followup
//		signal
//
Signal *
SignalClassifier::findFollowupSignal(Activity *act,
		SignalDescription& sig)
{
	Debug(DEBUG_SIGNAL_CLASS, sig.path.rfFreq, "findFollowup sigF =");
	// look at all the signals in the followup list, to see whether
	// this candidate matches any in the list.  Allow a certain amount
	// of error in the signal description
	Signal *signal;
	for (int32_t i = 0; (signal = act->getNthFollowupSignal(i)); ++i) {
		float64_t freq = signal->getRfFreq();
		float64_t lowFreq = freq - FOLLOW_UP_FREQ_RANGE;
		float64_t hiFreq = freq + FOLLOW_UP_FREQ_RANGE;
		float64_t sigFreq = sig.path.rfFreq;

		Debug(DEBUG_SIGNAL_CLASS, lowFreq, "findFollowup loF =");
		Debug(DEBUG_SIGNAL_CLASS, hiFreq, "findFollowup hiF =");
		if (sigFreq >= lowFreq && sigFreq <= hiFreq)
			return (signal);
	}
	return (0);
}

void
SignalClassifier::displayPulseSignal(PulseSignalHeader& signal)
{
	cout << "pulse signal" << endl;
	cout << signal << endl;
}

//
// apply the complete bad band list to the signal to see whether
//		it lies in a bad band region
//
bool_t
SignalClassifier::applyBadBands(Activity *act, SignalDescription& sig)
{
	CwBadBandList *cwBadBandList = act->getCwBadBandList();
	for (int32_t i = 0; i < cwBadBandList->getSize(); ++i) {
		::CwBadBand badBand = cwBadBandList->getNth(i);
		if (match(act, sig, badBand.band))
			return (SSE_TRUE);
	}
	for (int32_t i = 0; i < MAX_RESOLUTIONS; ++i) {
		PulseBadBandList *badBandList =
					act->getPulseBadBandList((Resolution) i);
		Assert(badBandList);
		for (int j = 0; j < badBandList->getSize(); ++j) {
			::PulseBadBand badBand = badBandList->getNth(j);
			if (match(act, sig, badBand.band))
				return (SSE_TRUE);
		}
	}
	return (SSE_FALSE);
}

bool
SignalClassifier::match(Activity *act, SignalDescription& sig,
		FrequencyBand& band)
{
	float64_t sigLo = sig.path.rfFreq - (HZ_TO_MHZ(sig.path.width / 2));
	float64_t sigHi = sig.path.rfFreq + (HZ_TO_MHZ(sig.path.width / 2));
	float64_t bandLo = band.centerFreq - band.bandwidth / 2;
	float64_t bandHi = band.centerFreq + band.bandwidth / 2;

	float64_t obsLen = act->getDataCollectionTime();
	if (sig.path.drift >= 0)
		sigHi += HZ_TO_MHZ(sig.path.drift * obsLen);
	else
		sigLo -= HZ_TO_MHZ(sig.path.drift * obsLen);
	return (sigLo < bandHi && sigHi > bandLo);
}

}
