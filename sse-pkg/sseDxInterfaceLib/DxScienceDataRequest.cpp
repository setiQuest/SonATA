/*******************************************************************************

 File:    DxScienceDataRequest.cpp
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


#include "sseDxInterfaceLib.h"
#include "Assert.h"

static string boolToOnOffString(bool value)
{
    if (value) 
        return "on";
    else
        return "off";
}

static const string SciDataRequestTypeNames[] = 
{
    "freq",    // REQ_FREQ
    "subchan"  // REQ_SUBCHANNEL
};

SciDataRequestType SseDxMsg::stringToSciDataRequestType(
    const string &reqTypeStr)
{
    for (int i=0; i< ARRAY_LENGTH(SciDataRequestTypeNames); i++)
    {
        if (SciDataRequestTypeNames[i] == reqTypeStr)
        {
            return static_cast<SciDataRequestType>(i);
        }
    }

    // string not found
    AssertMsg(0, "invalid science data request type");

    //throw 1;   // TBD use appropriate exception object
}

// Convert from enum to string.
//
string SseDxMsg::sciDataRequestTypeToString(
    SciDataRequestType reqType)
{
    if (reqType >= 0 && reqType < ARRAY_LENGTH(SciDataRequestTypeNames)) 
    { 
	return SciDataRequestTypeNames[reqType];
    }

    return "SseDxMsg Error: invalid science data request type";
    
    //AssertMsg(0, "invalid science data request type");

    //throw 1;   // TBD use appropriate exception object

}

static void marshallReqType(SciDataRequestType &requestType)
{
    requestType = static_cast<SciDataRequestType>(htonl(requestType));
}

ostream& operator << (ostream &strm, const DxScienceData &data)
{
    strm << "Dx Science Data: " << endl;
    strm << "--------------------------" << endl;

    Assert(data.requestType >= 0 &&
	   data.requestType < ARRAY_LENGTH(SciDataRequestTypeNames));
    strm << "  requestType:                 " <<
	SciDataRequestTypeNames[data.requestType] << endl;

    strm << "  subchan:                     "
	 << data.subchannel <<  endl;

    strm << "  rfFreq:                      "
	 << data.rfFreq << " MHz" << endl;
    strm << endl;
    return strm;
}

ostream& operator << (ostream &strm, const DxScienceDataRequest &dataRequest)
{
    strm << "Dx Science Data Request: " << endl;
    strm << "--------------------------" << endl;

    strm << "  sendBaselines:               "
	 << boolToOnOffString(dataRequest.sendBaselines) << endl;

    strm << "  sendBaselineStatistics:      "
	 << boolToOnOffString(dataRequest.sendBaselineStatistics) << endl;

    strm << "  checkBaselineWarningLimits:  "
	 << boolToOnOffString(dataRequest.checkBaselineWarningLimits) << endl;

    strm << "  checkBaselineErrorLimits:    "
	 << boolToOnOffString(dataRequest.checkBaselineErrorLimits) << endl;

    strm << "  baselineReportHalfFrames:    "
	 << dataRequest.baselineReportingHalfFrames << endl;

    strm << "  sendComplexAmplitudes:       " 
	 << boolToOnOffString(dataRequest.sendComplexAmplitudes) <<endl;;

    strm << dataRequest.scienceData << endl;
    return strm;
}

DxScienceData::DxScienceData()
    :
    requestType(REQ_FREQ),
    subchannel(0),
    rfFreq(0.0)
{
}

void DxScienceData::marshall()
{
    marshallReqType(requestType);
    NTOHL(subchannel);
    NTOHD(rfFreq);
}

void DxScienceData::demarshall()
{
    marshall();
}

DxScienceDataRequest::DxScienceDataRequest()
    :
    sendBaselines(SSE_FALSE),
    sendBaselineStatistics(SSE_FALSE),
    checkBaselineWarningLimits(SSE_FALSE),
    checkBaselineErrorLimits(SSE_FALSE),
    baselineReportingHalfFrames(0),
    sendComplexAmplitudes(SSE_FALSE)
{
}

void DxScienceDataRequest::marshall()
{
    SseMsg::marshall(sendBaselines);          // bool_t
    SseMsg::marshall(sendBaselineStatistics); // bool_t
    SseMsg::marshall(checkBaselineWarningLimits); // bool_t
    SseMsg::marshall(checkBaselineErrorLimits); // bool_t
    SseMsg::marshall(sendComplexAmplitudes);  // bool_t
    HTONL(baselineReportingHalfFrames);
    scienceData.marshall();
}

void DxScienceDataRequest::demarshall()
{
    marshall();
}

