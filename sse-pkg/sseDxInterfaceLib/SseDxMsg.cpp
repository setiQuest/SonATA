/*******************************************************************************

 File:    SseDxMsg.cpp
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


#include "SseDxMsg.h"
#include "Assert.h"
#include "HostNetByteorder.h"
#include "ArrayLength.h"
#include <iostream>
#include <string>
#include <iomanip>

// ------------- utility arrays & routines

// ------ Resolution --------------------
static const char *ResolutionStrings[] =
{
    "1 Hz", "2 Hz", "4 Hz", "8 Hz",    
    "16 Hz", "32 Hz", "64 Hz", "128 Hz",
    "256 Hz", "512 Hz", "1 KHz", "uninit"
 
};

string SseDxMsg::resolutionToString(Resolution res)
{
    if (res < 0 || res >= ARRAY_LENGTH(ResolutionStrings))
    {
	return "SseDxMsg Error: Invalid resolution";
    }

    return ResolutionStrings[res];
}

Resolution  SseDxMsg::stringToResolution(const string &resString)
{
    for (int i=0; i< ARRAY_LENGTH(ResolutionStrings); i++)
    {
        if (ResolutionStrings[i] == resString)
        {
            return static_cast<Resolution>(i);
        }
    }

    return RES_UNINIT;
}



//----------- SignalClass -------------------
static const char *SignalClassStrings[] =
{
    "Uninit", "Cand", "RFI", "Test", "Unkn"

};

string SseDxMsg::signalClassToString(SignalClass signalClass)
{
    if (signalClass < 0 || signalClass >= ARRAY_LENGTH(SignalClassStrings))
    {
	return "SseDxMsg Error: invalid SignalClass";
    }
    return SignalClassStrings[signalClass];
}

SignalClass SseDxMsg::stringToSignalClass(const string & classString)
{
    for (int i=0; i< ARRAY_LENGTH(SignalClassStrings); i++)
    {
        if (SignalClassStrings[i] == classString)
        {
            return static_cast<SignalClass>(i);
        }
    }

    return CLASS_UNINIT;
}


//----------- SignalClassReason -------------------
static const char *SignalClassReasonStrings[] =
{
  "CLASS_REASON_UNINIT", 

  "PASSED_POWER_THRESH",
  "PASSED_COHERENT_DETECT",

  "CONFIRM",
  "RECONFIRM",
  "NOT_SEEN_OFF",
  "SECONDARY_FOUND_SIGNAL",

  "SEEN_GRID_WEST",
  "NOT_SEEN_GRID_WEST",

  "SEEN_GRID_SOUTH",
  "NOT_SEEN_GRID_SOUTH",
  
  "SEEN_GRID_ON",
  "NOT_SEEN_GRID_ON",
  
  "SEEN_GRID_NORTH",
  "NOT_SEEN_GRID_NORTH",
  
  "SEEN_GRID_EAST",
  "NOT_SEEN_GRID_EAST",
  
  "GRID_PREDICTION",
    
  "ZERO_DRIFT",
  "RECENT_RFI_MATCH",
  "FAILED_COHERENT_DETECT",
  "FAILED_POWER_THRESH",
  "NO_SIGNAL_FOUND",
  "SNR_TOO_HIGH",
  "SNR_TOO_LOW",
  "DRIFT_TOO_HIGH",

  "SEEN_OFF", 
  "NO_RECONFIRM",
  "SEEN_MULTIPLE_BEAMS",
  "FALLS_IN_BAD_BAND",

  "FAILED_COHERENT_DETECT_GRID_WEST",
  "ZERO_DRIFT_GRID_WEST",
  "RECENT_RFI_MATCH_GRID_WEST",
  
  "FAILED_COHERENT_DETECT_GRID_SOUTH",
  "ZERO_DRIFT_GRID_SOUTH",
  "RECENT_RFI_MATCH_GRID_SOUTH",
  
  "FAILED_COHERENT_DETECT_GRID_ON",
  "ZERO_DRIFT_GRID_ON",
  "RECENT_RFI_MATCH_GRID_ON",
  
  "FAILED_COHERENT_DETECT_GRID_NORTH",
  "ZERO_DRIFT_GRID_NORTH",
  "RECENT_RFI_MATCH_GRID_NORTH",
  
  "FAILED_COHERENT_DETECT_GRID_EAST",
  "ZERO_DRIFT_GRID_EAST",
  "RECENT_RFI_MATCH_GRID_EAST",

  "TEST_SIGNAL_MATCH",

  "TOO_MANY_CANDIDATES",
  "BIRDIE_SCAN",
  "RFI_SCAN",
  "SECONDARY_NO_SIGNAL_FOUND",
  "SIGNAL_NOT_IN_CHANNEL"

};

string SseDxMsg::signalClassReasonToString(SignalClassReason classReason)
{
    Assert(ARRAY_LENGTH(SignalClassReasonStrings) == SIGNAL_CLASS_REASON_END);

    if (classReason < 0 ||
	classReason >= ARRAY_LENGTH(SignalClassReasonStrings))
    {
	return "SseDxMsg Error:  SignalClassReason out of range";
    }
    return SignalClassReasonStrings[classReason];
}

static const char *BriefSignalClassReasonStrings[] =
{
    "Uninit",  // "CLASS_REASON_UNINIT", 

    "PsPwrT", // "PASSED_POWER_THRESH",
    "PsCohD", // "PASSED_COHERENT_DETECT",

    "Confrm", // "CONFIRM",
    "RConfrm", // "RECONFIRM",
    "NtSnOff", // "NOT_SEEN_OFF",
    "SSawSig", // "SECONDARY_FOUND_SIGNAL",

    "SeenWst", //  "SEEN_GRID_WEST",
    "NtSnWst", // "NOT_SEEN_GRID_WEST",

    "SeenSou", // "SEEN_GRID_SOUTH",
    "NtSnSou",  // "NOT_SEEN_GRID_SOUTH",
  
    "SnGrOn",  // "SEEN_GRID_ON",
    "NtSnGOn", // "NOT_SEEN_GRID_ON",
  
    "SeenNor", // "SEEN_GRID_NORTH",
    "NtSnNor", // "NOT_SEEN_GRID_NORTH",
  
    "SeenEst", // "SEEN_GRID_EAST",
    "NtSnEst", // "NOT_SEEN_GRID_EAST",
  
    "GrPrdct", // "GRID_PREDICTION",

    "ZeroDft", // "ZERO_DRIFT",
    "RctRFI", // "RECENT_RFI_MATCH",
    "FdCohD", // "FAILED_COHERENT_DETECT",
    "FdPwrT", // FAILED_POWER_THRESH",
    "NoSignl", // "NO_SIGNAL_FOUND",
    "SNR2Hi", // "SNR_TOO_HIGH",
    "SNR2Lo", // "SNR_TOO_LOW",
    "Dft2Hi", // "DRIFT_TOO_HIGH",

    "SeenOff", // "SEEN_OFF", 
    "NoReCfm", // "NO_RECONFIRM",
    "SnMulBm", // "SEEN_MULTIPLE_BEAMS",
    "InBdBnd", // "FALLS_IN_BAD_BAND",

    "FdCoWst", // "FAILED_COHERENT_DETECT_GRID_WEST",
    "ZDftWst", // "ZERO_DRIFT_GRID_WEST",
    "RRFIWst", // "RECENT_RFI_MATCH_GRID_WEST",
  
    "FdCoSou", // "FAILED_COHERENT_DETECT_GRID_SOUTH",
    "ZDftSou", // "ZERO_DRIFT_GRID_SOUTH",
    "RRFISou", // "RECENT_RFI_MATCH_GRID_SOUTH",
  
    "FdCoGOn", //  "FAILED_COHERENT_DETECT_GRID_ON",
    "ZDftGOn", //  "ZERO_DRIFT_GRID_ON",
    "RRFIGOn", //  "RECENT_RFI_MATCH_GRID_ON",
  
    "FdCoNor", // "FAILED_COHERENT_DETECT_GRID_NORTH",
    "ZDftNor", // "ZERO_DRIFT_GRID_NORTH",
    "RRFINor", // "RECENT_RFI_MATCH_GRID_NORTH",
  
    "FdCoEst", // "FAILED_COHERENT_DETECT_GRID_EAST",
    "ZDftEst", //  "ZERO_DRIFT_GRID_EAST",
    "RRFIEst", // "RECENT_RFI_MATCH_GRID_EAST",

    "TestSig", // "TEST_SIGNAL_MATCH",

    "2MnyCnd", // "TOO_MANY_CANDIDATES",
    "BrdScan", // "BIRDIE_SCAN",
    "RFIScan", // "RFI_SCAN"
    "SNoSigl", // "SECONDARY_NO_SIGNAL_FOUND"
    "SNtInCh", // "SIGNAL_NOT_IN_CHANNEL"

};

string SseDxMsg::signalClassReasonToBriefString(
    SignalClassReason classReason)
{
    Assert(ARRAY_LENGTH(BriefSignalClassReasonStrings) == SIGNAL_CLASS_REASON_END);

    if (classReason < 0 || classReason >= ARRAY_LENGTH(BriefSignalClassReasonStrings))
    {
	return "SseDxMsg Error:  SignalClassReason out of range";
    }

    return BriefSignalClassReasonStrings[classReason];
}

SignalClassReason SseDxMsg::briefStringToSignalClassReason(
    const string & briefSigClassString)
{
    for (int i=0; i< ARRAY_LENGTH(BriefSignalClassReasonStrings); i++)
    {
        if (BriefSignalClassReasonStrings[i] == briefSigClassString)
        {
            return static_cast<SignalClassReason>(i);
        }
    }

    return CLASS_REASON_UNINIT;
}



// names associated with SSE - DX message codes
static const char *codeNames[] =
{
    "message_code_uninit",   // invalid ("uninitialized") message code
    "request intrinsics",
    "send intrinsics",
    "configure dx",
    "perm RFI mask",
    "birdie mask",
    "rcvr birdie mask",
    "recent rfi mask",
    "test signal mask",
    "request dx status",
    "send dx status",
    "send dx activity parameters",
    "dx tuned",
    "dx science data request",
    "start time",
    "baseline_init_accum_started",
    "baseline_init_accum_complete",
    "data collection started",
    "data collection complete",
    "signal_detection_started",
    "signal_detection_complete",
    "begin_sending_candidates",
    "send_candidate_cw_power_signal",
    "send_candidate_pulse_signal",
    "done_sending_candidates",
    "begin_sending_signals",
    "send_cw_power_signal",
    "send_pulse_signal",
    "done_sending_signals",
    "begin_sending_cw_coherent_signals",
    "send_cw_coherent_signal",
    "done_sending_cw_coherent_signals",
    "begin_sending_candidate_results",
    "send_cw_coherent_candidate_result",
    "send_pulse_candidate_result",
    "done_sending_candidate_results",
    "begin_sending_follow_up_signals",
    "send_follow_up_cw_signal",
    "send_follow_up_pulse_signal",
    "done_sending_follow_up_signals",
    "request_archive_data",
    "discard_archive_data",
    "archive_signal",
    "begin_sending_archive_complex_amplitudes",
    "send_archive_complex_amplitudes",
    "done_sending_archive_complex_amplitudes",
    "archive_complete",
    "send_dx_message",
    "send_baseline",
    "send_complex_amplitudes",
    "stop_dx_activity",
    "shutdown_dx",
    "restart_dx",
    "dx_activity_complete",
    "send_baseline_statistics",
    "baseline_warning_limits_exceeded",
    "baseline_error_limits_exceeded",
    "begin_sending_bad_bands",
    "send_pulse_bad_band",
    "send_cw_bad_band",
    "done_sending_bad_bands"
};


string SseDxMsg::messageCodeToString(uint32_t code)
{
    AssertMsg(ARRAY_LENGTH(codeNames) == 
	      DX_MESSAGE_CODE_END - DX_CODE_RANGE_START,
	      "length of codeNames array differs from enum list length");

    // dx message codes are offset from zero, so adjust the
    // code to fit the codeNames array.

    int index = static_cast<signed int>(code) - DX_CODE_RANGE_START;
    if (index > 0 && index < ARRAY_LENGTH(codeNames)) 
    { 
	return codeNames[index];
    }
    else
    {
	return "Error: message code out of range";
    }
}


//--------------------------------------------------
// marshall & demarshall some enums.
// These casts keep the compiler from issuing warnings.

//----------- 
void SseDxMsg::marshall(Resolution &arg)
{
    arg = static_cast<Resolution>(htonl(arg));
}

void SseDxMsg::demarshall(Resolution &arg)
{
    marshall(arg);
}

//----------- 
void SseDxMsg::marshall(SignalClass &arg)
{
    arg = static_cast<SignalClass>(htonl(arg));
}

void SseDxMsg::demarshall(SignalClass &arg)
{
    marshall(arg);
}
//----------- 
void SseDxMsg::marshall(SignalClassReason &arg)
{
    arg = static_cast<SignalClassReason>(htonl(arg));
}

void SseDxMsg::demarshall(SignalClassReason &arg)
{
    marshall(arg);
}

//----------- 
void SseDxMsg::marshall(DxActivityState &arg)
{
    arg = static_cast<DxActivityState>(htonl(arg));
}

void SseDxMsg::demarshall(DxActivityState &arg)
{
    marshall(arg);
}

