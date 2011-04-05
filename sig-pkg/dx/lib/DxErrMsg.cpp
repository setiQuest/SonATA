/*******************************************************************************

 File:    DxErrMsg.cpp
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
// DX-specific error messages
//
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/DxErrMsg.cpp,v 1.3 2009/03/11 01:31:49 kes Exp $
//
#include "ErrMsg.h"
#include "DxErr.h"
#include "DxErrMsg.h"

namespace sonata_lib {

ErrMsgList dxList[] = {
	{ (ErrCode) ERR_NDC, "no data collection" },
	{ (ErrCode) ERR_NI, "not idle" },
	{ (ErrCode) ERR_ANI, "activity not idle" },
	{ (ErrCode) ERR_NFA, "no free activities" },
	{ (ErrCode) ERR_NSA, "no such activity" },
	{ (ErrCode) ERR_NIA, "no idle activity" },
	{ (ErrCode) ERR_DCIP, "data collection in progress" },
	{ (ErrCode) ERR_NAD, "no activity defined" },
	{ (ErrCode) ERR_ADM, "activity definition mismatch" },
	{ (ErrCode) ERR_ANR, "activity not running" },
	{ (ErrCode) ERR_IAS, "invalid activity state" },
	{ (ErrCode) ERR_AAD, "activity already defined" },
	{ (ErrCode) ERR_NAA, "no activity allocated" },
	{ (ErrCode) ERR_AIC, "activity in collection" },
	{ (ErrCode) ERR_ANS, "activity not started" },
	{ (ErrCode) ERR_ISF, "invalid subchannel frequency" },
	{ (ErrCode) ERR_NSP, "NULL signal pointer" },
	{ (ErrCode) ERR_IST, "invalid signal type" },
	{ (ErrCode) ERR_NCS, "no candidate signal" },
	{ (ErrCode) ERR_IAL, "invalid activity length" },
	{ (ErrCode) ERR_IPT, "invalid polarization type" },
	{ (ErrCode) ERR_SOE, "start obs error" },
	{ (ErrCode) ERR_CNI, "confirmation channel not initialized" },
	{ (ErrCode) ERR_ICW, "invalid channel width" },
	{ (ErrCode) ERR_STAP, "start time already past" },
	{ (ErrCode) ERR_IM, "invalid mode" },
	{ (ErrCode) ERR_AICF, "activity in confirmation" },
	{ (ErrCode) ERR_AIA, "activity in archiving" },
	{ (ErrCode) ERR_ISC, "incorrect signal count" },
	{ (ErrCode) ERR_NAIA, "no activity in archive" },
	{ (ErrCode) ERR_ANIA, "activity not in archive" },
	{ (ErrCode) ERR_NAC, "no archive connection" },
	{ (ErrCode) ERR_IR, "invalid resolution" },
	{ (ErrCode) ERR_BLE, "baseline limits error" },
	{ (ErrCode) ERR_CAA, "channel already allocated" },
	{ (ErrCode) ERR_IBO, "input buffer overflow" },
	{ (ErrCode) ERR_PLE, "pulse limits exceeded" },
	{ (ErrCode) ERR_CAI, "channel already initialized" },
	{ (ErrCode) ERR_DCC, "data collection complete" },
	{ (ErrCode) ERR_DSP, "duplicate singleton pulse" },
	{ (ErrCode) ERR_DPF, "duplicate pulse found" },
	{ (ErrCode) ERR_SNC, "signal is not a candidate" },
	{ (ErrCode) ERR_NP2, "# of frames not a power of 2" },
	{ (ErrCode) ERR_SWA, "starting wrong activity" },
	{ (ErrCode) ERR_DFP, "DFB is pending" },
	{ (ErrCode) ERR_IPFM, "SSE simulator input file missing" },
	{ (ErrCode) ERR_IPIV, "SSE simulator input file invalid" },
	{ (ErrCode) ERR_TMF, "too many frames" },
	{ (ErrCode) ERR_INS, "invalid number of archived subchannels" },
	{ (ErrCode) ERR_IPV, "invalid packet version" },
	{ (ErrCode) ERR_DCE, "data collection error" },
	{ (ErrCode) ERR_NPRR, "no packets received, restarting" },
	{ (ErrCode) ERR_ASM, "all subchannels masked" },
	{ (ErrCode) ERR_PSU, "R&L packet streams are unsynchronized" },
	{ (ErrCode) ERR_SOC, "Signal lies outside channel" },


	{ ERR_END, "" }
};

void buildErrList()
{
	ErrMsg *errList = ErrMsg::getInstance();
	errList->addErrs((ErrMsgList *) dxList);
}

}
