/*******************************************************************************

 File:    DxErr.h
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
// DX-specific error codes
//
// $Header: /home/cvs/nss/sonata-pkg/dx/include/DxErr.h,v 1.3 2009/03/11 01:30:12 kes Exp $
//
#ifndef _DxErrH
#define _DxErrH

#include "Err.h"

namespace sonata_lib {

// internal error codes

enum DxErrCode {
	// Channelizer error codes
	ERR_NI = ERR_END + 1,			// not idle
	ERR_ANI,						// activity not idle
	ERR_NFA, 						// no free activities
	ERR_NSA,						// no such activity
	ERR_NIA,						// no idle activity
	ERR_DCIP,						// data collection in progress
	ERR_NAD,						// no activity defined
	ERR_ADM,						// activity definition mismatch
	ERR_ANR,						// activity not running
	ERR_IAS,						// invalid activity state
	ERR_AAD,						// activity already defined
	ERR_NAA,						// no activity allocated
	ERR_AIC,						// activity in collection
	ERR_ANS,						// activity not started
	ERR_ISF,						// invalid subchannel frequency
	ERR_NSP,						// null signal pointer
	ERR_IST,						// invalid signal type
	ERR_NCS,						// no candidate signal
	ERR_IAL,						// invalid activity length
	ERR_SOE,						// start obs error
	ERR_CNI,						// confirmation channel not initialized
	ERR_ICW,						// invalid channel width
	ERR_STAP,						// start time already past
	ERR_IM,							// invalid mode
	ERR_AICF,						// activity in confirmation
	ERR_AIA,						// activity in archiving
	ERR_ISC,						// incorrect signal count
	ERR_NAIA,						// no activity in archive
	ERR_ANIA,						// activity not in archive
	ERR_NAC,						// no archive connection
	ERR_IR,							// invalid resolution
	ERR_BLE,						// baseline limits error
	ERR_CAA,						// channel already allocated
	ERR_IBO,						// input buffer overflow
	ERR_PLE,						// pulse limits error,
	ERR_CAI,						// channel already initialized
	ERR_DCC,						// data collection complete
	ERR_DSP,						// duplicate singleton pulse
	ERR_DPF,						// duplicate pulse found
	ERR_SNC,						// signal is not a candidate
	ERR_NP2,						// # of frames is not a power of 2
	ERR_SWA,						// starting wrong activity
	ERR_DFP,						// DFB pending
	ERR_IPFM,						// SSE simulator input file missing
	ERR_IPIV,						// SSE simulator input invalid
	ERR_TMF,						// too many frames
	ERR_INS,						// invalid number of archived subchannels
	ERR_IPV,						// invalid packet version
	ERR_DCE,						// data collection error
	ERR_NPRR,						// no packet received, restarting
	ERR_ASM,						// all subchannels masked
	ERR_PSU,							// packet streams unsynchronized
	ERR_SOC							// signal lies outside channel
};

}

#endif
