/*******************************************************************************

 File:    SubchannelMask.h
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
// Subchannel mask class definition
//
// $Header: /home/cvs/nss/sonata-pkg/dx/include/SubchannelMask.h,v 1.3 2009/02/22 04:37:45 kes Exp $
//
#ifndef _SubchannelMaskH
#define _SubchannelMaskH

#include <bitset>
#include "BirdieMask.h"
#include "PermRfiMask.h"
#include "RecentRfiMask.h"
#include "TestSignalMask.h"

namespace dx {

typedef std::bitset<MAX_SUBCHANNELS> SubchannelMaskBitset;

// forward declaration
class Activity;

class SubchannelMask {
public:
	SubchannelMask(SubchannelMaskBitset *mask_ = 0);
	~SubchannelMask();

	void init(Activity *act);

	void createSubchannelMask(Activity *act);
	void maskSubchannel(int32_t subchannel);
	void maskSubchannel(const DxScienceData *scienceData);
//	int32_t getSubchannelMaskLen() { return (mask.size()); }
	void getSubchannelMask(SubchannelMaskBitset *mask_);
	bool isSubchannelMasked(int32_t subchannel);
	bool noSubchannelsMasked();
	bool allSubchannelsMasked();
	int32_t getCount() { return (mask.count()); }

	void applyRecentRfiMask(RecentRfiMask *recentRfiMask, float64_t centerFreq);

private:
	int32_t subchannels;
	float64_t subchannelBandwidth;
	SubchannelMaskBitset mask;

	void applyBirdieMask(BirdieMask *birdieMask, float64_t dxIfcFreq);
	void applyPermRfiMask(PermRfiMask *permRfiMask, float64_t centerFreq);
	void applyTestSignalMask(TestSignalMask *testSignalMask,
			float64_t centerFreq);
	void computeSubchannelRange(FrequencyBand& band, int32_t& first,
			int32_t& last);
	int32_t computeSubchannel(float64_t freq);

	// forbidden
	SubchannelMask(const SubchannelMask&);
	SubchannelMask& operator=(const SubchannelMask&);
};

}

#endif
