/*******************************************************************************

 File:    SubchannelMask.cpp
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
// Subchannel mask class
//
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/SubchannelMask.cpp,v 1.5 2009/05/24 23:44:26 kes Exp $
//
#include <sseDxInterface.h>
#include <DxOpsBitset.h>
#include "System.h"
#include "Activity.h"
#include "Err.h"
#include "State.h"
#include "SubchannelMask.h"

namespace dx {

SubchannelMask::SubchannelMask(SubchannelMaskBitset *mask_):
		subchannels(0), subchannelBandwidth(0)
{
	if (mask_)
		mask = *mask_;
	else
		mask.reset();
}

SubchannelMask::~SubchannelMask()
{
}

/**
 * Initialize a subchannel mask.
 *
 * Description:\n
 * 	Resets all the mask bits, and sets the number of subchannels and the
 *	subchannel bandwidth.
 */
void
SubchannelMask::init(Activity *act)
{
	Assert(act);
	subchannels = act->getUsableSubchannels();
	subchannelBandwidth = act->getSubchannelWidthMHz();
	mask.reset();
}
//
// createSubchannelMask: create the DSP subchannel mask
//
// Synopsis:
//		void createSubchannelMask(subchannels_, birdieMask_, permRfiMask_,
//				testSignalMask_);
//		int32_t subchannels_;				# of subchannels in use
//		DxBirdieMask *birdieMask_;		birdie mask
//		DxPermRfIMask *permRfiMask_;	permanent RFI mask
//		DxTestSignalMask *testSignalMask_;	test signal mask
// Notes:
//		The active set of subchannels is specified by subchannels_;
//		all DX subchannels which lie outside this range are
//		masked.
//		Masking is performed by setting the bit representing
//		that subchannel to 1.
//		The birdie mask is an IF mask, which means it is independent
//		of RF frequency.
//		The permanent RFI mask is an RF mask which requires that
//		the center frequency be known.
//		The test signal mask works in reverse: it resets any
//		subchannel specified.
void
SubchannelMask::createSubchannelMask(Activity *act)
{
	Assert(act);
	subchannels = act->getUsableSubchannels();
	subchannelBandwidth = act->getSubchannelWidthMHz();

	Debug(DEBUG_SUBCHANNEL, act->getActivityId(), "actId");
	Debug(DEBUG_SUBCHANNEL, act->getSkyFreq(), "dxSkyFreq");
	Debug(DEBUG_SUBCHANNEL, act->getIfcSkyFreq(), "ifcSkyFreq");
	Debug(DEBUG_SUBCHANNEL, act->getRcvrSkyFreq(), "rcvrSkyFreq");
	float64_t dxSkyFreq = act->getSkyFreq();
	float64_t dxIfcFreq = dxSkyFreq - act->getIfcSkyFreq();
	float64_t dxRcvrFreq = dxSkyFreq - act->getRcvrSkyFreq();
	BirdieMask *birdieMask = act->getBirdieMask();
	BirdieMask *rcvrBirdieMask = act->getRcvrBirdieMask();
	PermRfiMask *permRfiMask = act->getPermRfiMask();
	TestSignalMask *testSignalMask = act->getTestSignalMask();
	uint32_t ops = act->getOperationsMask();
	Debug(DEBUG_SUBCHANNEL, (int32_t) ops, "operations");
	Debug(DEBUG_BIRDIE_MASK, act->getRcvrSkyFreq(),
				"...act->getRcvrSkyFrequency()");
	DxOpsBitset operations(ops);
	ops = operations.to_ulong();
	Debug(DEBUG_SUBCHANNEL, (int32_t) ops, "operations");

	mask.reset();

	Debug(DEBUG_SUBCHANNEL, 0, "birdieMask");
	if (operations.test(APPLY_BIRDIE_MASK))
		applyBirdieMask(birdieMask, dxIfcFreq);
	Debug(DEBUG_SUBCHANNEL, 0, "rcvrBirdieMask");
	if (operations.test(APPLY_RCVR_BIRDIE_MASK))
		applyBirdieMask(rcvrBirdieMask, dxRcvrFreq);
	Debug(DEBUG_SUBCHANNEL, 1, "permRfiMask");
	if (operations.test(APPLY_PERMANENT_RFI_MASK))
		applyPermRfiMask(permRfiMask, dxSkyFreq);
	Debug(DEBUG_SUBCHANNEL, 2, "testSignalMask");
	if (operations.test(APPLY_TEST_SIGNAL_MASK))
		applyTestSignalMask(testSignalMask, dxSkyFreq);
}

/**
 * Mask a subchannel.
 */
void
SubchannelMask::maskSubchannel(int32_t subchannel)
{
	if (subchannel >= 0 && subchannel < subchannels)
		mask.set(subchannel);
}

void
SubchannelMask::maskSubchannel(const DxScienceData *scienceData)
{
	int32_t subchannel = scienceData->subchannel;
	if (scienceData->requestType == REQ_FREQ)
		subchannel = computeSubchannel(scienceData->rfFreq);
	maskSubchannel(subchannel);
}

/**
 * Return the subchannel mask.
 */
void
SubchannelMask::getSubchannelMask(SubchannelMaskBitset *mask_)
{
	Assert(mask_);
	*mask_ = mask;
}

/**
 * Test whether the specified subchannel is masked.
 */
bool
SubchannelMask::isSubchannelMasked(int32_t subchannel)
{
	return (mask.test(subchannel));
}

/**
 * Test whether any subchannels are masked.
 */
bool
SubchannelMask::noSubchannelsMasked()
{
	return (mask.none());
}

/**
 * Test whether all subchannels are masked.
 *
 * Description:\n
 * 	Returns true if all subchannels have been masked.\n\n
 * Notes:\n
 * 	Used to determine whether the entire channel has been disabled;
 * 	this is a warning condition.
 */
bool
SubchannelMask::allSubchannelsMasked()
{
	return (mask.count() >= (size_t) subchannels);
}

//
// applyBirdieMask: apply the specified birdie mask to the subchannel mask
//
// Synopsis:
//		void applyBirdieMask(birdieMask, dxIfcFreq);
//		DxBirdieMask *birdieMask;		active birdie mask
//		float64_t dxIfcFreq;			DX center freq relative to IFC ctr freq
// Notes:
//		The birdie mask comes in two versions: an IF mask which is
//		independent of the RF center frequency, and an RF mask which
//		is used to mask birdies in the RF (receiver).
//		Since the IF bandwidth is greater than the DX bandwidth,
//		these values must be adjusted for the DX's center frequency
//		relative to the IFC center frequency.
//
void
SubchannelMask::applyBirdieMask(BirdieMask *birdieMask,
		float64_t dxIfcFreq)
{
	if (!birdieMask)
		return;
	Debug(DEBUG_BIRDIE_MASK, dxIfcFreq, "dx ctr freq ofs");
	for (const FrequencyBand *band = birdieMask->getFirst(); band;
			band = birdieMask->getNext()) {
		FrequencyBand freqBand = *band;
		Debug(DEBUG_BIRDIE_MASK, freqBand.centerFreq, "ifc ctr freq");
		freqBand.centerFreq -= dxIfcFreq;
		Debug(DEBUG_BIRDIE_MASK, freqBand.centerFreq, "dx ifc freq");
		int32_t first, last;
		computeSubchannelRange(freqBand, first, last);
		Debug(DEBUG_BIRDIE_MASK, first, "first subchannel");
		Debug(DEBUG_BIRDIE_MASK, last, "last subchannel");
		for (int32_t i = first; i <= last; ++i)
			mask.set(i);
	}
}

void
SubchannelMask::applyPermRfiMask(PermRfiMask *permRfiMask,
		float64_t centerFreq)
{
	if (!permRfiMask)
		return;
	for (FrequencyBand *band = permRfiMask->getFirst(); band;
			band = permRfiMask->getNext()) {
		FrequencyBand freqBand = *band;
		Debug(DEBUG_SUBCHANNEL, centerFreq, "rf center freq");
		Debug(DEBUG_SUBCHANNEL, freqBand.centerFreq, "mask center freq");
		freqBand.centerFreq -= centerFreq;
		int32_t first, last;
		computeSubchannelRange(freqBand, first, last);
		Debug(DEBUG_SUBCHANNEL, first, "first subchannel");
		Debug(DEBUG_SUBCHANNEL, last, "last subchannel");
		for (int32_t i = first; i <= last; ++i)
			mask.set(i);
	}
}

void
SubchannelMask::applyTestSignalMask(TestSignalMask *testSignalMask,
		float64_t centerFreq)
{
	if (!testSignalMask)
		return;
	for (FrequencyBand *band = testSignalMask->getFirst(); band;
			band = testSignalMask->getNext()) {
		FrequencyBand freqBand = *band;
		freqBand.centerFreq -= centerFreq;
		int32_t first, last;
		computeSubchannelRange(freqBand, first, last);
		for (int32_t i = first; i <= last; ++i)
			mask.reset(i);
	}
}

void
SubchannelMask::applyRecentRfiMask(RecentRfiMask *recentRfiMask,
		float64_t centerFreq)
{
	if (!recentRfiMask)
		return;
	for (FrequencyBand *band = recentRfiMask->getFirst(); band;
			band = recentRfiMask->getNext()) {
		FrequencyBand freqBand = *band;
		Debug(DEBUG_SUBCHANNEL, centerFreq, "rf center freq");
		Debug(DEBUG_SUBCHANNEL, freqBand.centerFreq, "mask center freq");
		freqBand.centerFreq -= centerFreq;
		int32_t first, last;
		computeSubchannelRange(freqBand, first, last);
		Debug(DEBUG_SUBCHANNEL, first, "first subchannel");
		Debug(DEBUG_SUBCHANNEL, last, "last subchannel");
		for (int32_t i = first; i <= last; ++i)
			mask.set(i);
	}
}

//
// computeSubchannelRange: compute the set of subchannels covered by a
//		frequency band
//
// Notes:
//		Computes the entire set of subchannels covered by a
//		frequency band.
//
void
SubchannelMask::computeSubchannelRange(FrequencyBand& band,
		int32_t& first, int32_t& last)
{
	float64_t loFreq = band.centerFreq - band.bandwidth / 2;
	float64_t hiFreq = band.centerFreq + band.bandwidth / 2;

	first = computeSubchannel(loFreq);
	if (first < 0)
		first = 0;
	last = computeSubchannel(hiFreq);
	if (last >= subchannels)
		last = subchannels - 1;
}

/**
 * Compute the subchannel corresponding to the specified frequency.
 *
 * Description:\n
 * 	Given an IF frequency relative to the DX center frequency, compute
 * 	the subchannel in which the frequency lies.\n\n
 *
 * Notes:\n
 * 	Since DC lies in the center of subchannel n/2, we have to adjust
 * 	the frequency by half a subchannel to get the correct value.
 */
int32_t
SubchannelMask::computeSubchannel(float64_t freq_)
{
	Debug(DEBUG_SUBCHANNEL, freq_, "freq");
	freq_ += (freq_ < 0 ? -subchannelBandwidth / 2 : subchannelBandwidth / 2);
	int32_t subchannel = (int32_t) (freq_ / subchannelBandwidth);
	subchannel += subchannels / 2;
	Debug(DEBUG_SUBCHANNEL, freq_, "freq");
	Debug(DEBUG_SUBCHANNEL, subchannel, "subchannel");
	return (subchannel);
}

}
