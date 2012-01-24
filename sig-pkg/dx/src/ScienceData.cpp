/*******************************************************************************

 File:    ScienceData.cpp
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
// Science data.  Part of Spectrometer class.
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/ScienceData.cpp,v 1.6 2009/03/06 22:14:51 kes Exp $
//
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <DxOpsBitset.h>
#include "DxErr.h"
#include "Log.h"
#include "Spectrometer.h"
//#include "SignalIdGenerator.h"

namespace dx {

/**
 * Science data functions.
 */
void
Spectrometer::sendScienceData(Polarization pol, int32_t hf)
{
	sendBaselineData(pol, hf);
	sendComplexAmplitudes(pol, hf);
}

/**
 * Send baseline information.
 */
void
Spectrometer::sendBaselineData(Polarization pol, int32_t hf)
{
	// if we're still baselining or not a baseline half frame, no report
	if (hf < 0 || hf % baselineReportingRate)
		return;

	// time to report half frame data
	computeBaselineStats(pol);

	const DxActivityParameters& params = activity->getActivityParams();
	const DxScienceDataRequest *scienceData = activity->getScienceData();
	DxOpsBitset operations(params.operations);

	blStats.status = BASELINE_STATUS_GOOD;
	string s;
	if (scienceData->checkBaselineErrorLimits
			&& checkBaselineLimits(params.baselineErrorLimits, s)) {
		blStats.status = BASELINE_STATUS_ERROR;
		sendBaselineMsg(pol, BASELINE_ERROR_LIMITS_EXCEEDED, s);
	}
	else if (scienceData->checkBaselineWarningLimits
			&& checkBaselineLimits(params.baselineWarningLimits, s)) {
		blStats.status = BASELINE_STATUS_WARNING;
		sendBaselineMsg(pol, BASELINE_WARNING_LIMITS_EXCEEDED, s);
	}

	if (!scienceData->sendBaselines)
		return;

	if  (scienceData->sendBaselineStatistics)
		sendBaselineStatistics();

	// send the baseline statistics
	size_t len = sizeof(BaselineHeader) + subchannels * sizeof(BaselineValue);
	MemBlk *blk = partitionSet->alloc(len);
	Assert(blk);
	BaselineHeader *blHdr = static_cast<BaselineHeader *> (blk->getData());
	BaselineValue *blArray = reinterpret_cast<BaselineValue *> (blHdr + 1);
	float32_t *blData = channel->getBlData(pol);
	for (int32_t i = 0; i < subchannels; ++i) {
		if (blData[i])
			blArray[i].value = 1.0 / blData[i];
		else
			blArray[i].value = 0.0;
		blArray[i].value *= blArray[i].value;
	}
	// build the header
	blHdr->rfCenterFreq = activity->getSkyFreq();
	blHdr->bandwidth = channel->getChannelWidthMHz();
	blHdr->halfFrameNumber = hf;
	blHdr->numberOfSubchannels = subchannels;
	blHdr->pol = pol;
	blHdr->activityId = activity->getActivityId();

	// send the message
	Msg *msg = msgList->alloc(SEND_BASELINE, activity->getActivityId(), blHdr,
			len, blk);
	respQ->send(msg);
}

/**
 * Compute baseline statistics.
 *
 * Description:\n
 * 	Computes baseline statistics for the current baselines, including mean,
 * 	min, max, and standard deviation.  The computation is performed using
 * 	all active (i.e., non-masked) subchannels.
 */
void
Spectrometer::computeBaselineStats(Polarization pol)
{
	int32_t nzSubchannels = 0;
	float32_t sum = 0, min = 1e30, max = 0;
	float32_t varray[MAX_SUBCHANNELS];
	float32_t *blData = channel->getBlData(pol);
	for (int32_t i = 0; i < subchannels; ++i) {
		if (activity->isSubchannelMasked(i))
			varray[i] = 0;
		else {
			varray[i] = blData[i] ? 1.0 / blData[i] : 0;
			varray[i] *= varray[i];
			sum += varray[i];
			if (varray[i] < min)
				min = varray[i];
			if (varray[i] > max)
				max = varray[i];
			++nzSubchannels;
		}
	}
	if (nzSubchannels < 2)
		return;
	float32_t mean = sum / nzSubchannels;

	// compute the standard deviation
	sum = 0;
	for (int32_t i = 0; i < subchannels; ++i) {
		if (varray[i]) {
			float32_t val = mean - varray[i];
			sum += val * val;
		}
	}
	blStats.mean = mean;
	blStats.stdDev = sqrt(sum / (nzSubchannels - 1));
	blStats.range = max - min;
	blStats.halfFrameNumber = halfFrame;
	blStats.rfCenterFreqMhz = activity->getSkyFreq();
	blStats.bandwidthMhz = channel->getChannelWidthMHz();
	blStats.pol = pol;
}

Error
Spectrometer::checkBaselineLimits(const BaselineLimits& limits, string& s)
{
	Error err = ERR_BLE;

	// test the baseline values against the limits
	std::stringstream ss;
	if (blStats.mean < limits.meanLowerBound
			|| blStats.mean > limits.meanUpperBound) {
		ss << "mean out of range " << blStats.mean << " [";
		ss << limits.meanLowerBound << ", " << limits.meanUpperBound << "]";
		ss << endl;
		s = ss.str();
	}
	else if (blStats.range > limits.maxRange) {
		ss << "range too large " << blStats.range << " [";
		ss << limits.maxRange << "]" << endl;
		s = ss.str();
	}
	else {
		float32_t sdPercent = (blStats.stdDev / blStats.mean) * 100;
		if (sdPercent > limits.stdDevPercent) {
			ss << "stdDev out of range, pct of mean " << sdPercent;
			ss << " [" << limits.stdDevPercent << "]" << endl;
			s = ss.str();
		}
		else
			err = 0;
	}
	return (err);
}

/**
 * Send a baseline error message to the SSE.
 *
 * Description:\n
 * 	Called whenever the baseline statistics exceed limits specified by the
 * 	SSE.  The message can be either a warning or an error, depending on the
 * 	limit set exceeded.
 */
void
Spectrometer::sendBaselineMsg(Polarization pol, DxMessageCode code,
		const string& s)
{
	size_t len = sizeof(BaselineLimitsExceededDetails);
	MemBlk *blk = partitionSet->alloc(len);
	Assert(blk);
	BaselineLimitsExceededDetails *details
			= static_cast<BaselineLimitsExceededDetails *> (blk->getData());
	details->pol = pol;
	strncpy(details->description, s.c_str(), MAX_NSS_MESSAGE_STRING);

	Msg *msg = msgList->alloc(code, activity->getActivityId(), details, len,
			blk);
	respQ->send(msg);
}

/**
 * Send complex amplitudes for the waterfall subchannel.
 */
void
Spectrometer::sendComplexAmplitudes(Polarization pol, int32_t hf)
{
	if (hf < 0)
		return;

	const DxActivityParameters& params = activity->getActivityParams();
	const DxScienceDataRequest& scienceData =  params.scienceDataRequest;
	int32_t subchannel = -1;
	if (scienceData.sendComplexAmplitudes)
		subchannel = sendSubchannelData(pol, hf, scienceData.scienceData);

	// if this is a ZX, send requested subchannels to the SSE, but don't
	// resend the science data subchannel
	if (args->zxMode()) {
		int32_t end = channel->getUsableSubchannels();
		for (int32_t i = 0; i < end; ++i) {
			if (activity->isSubchannelRequested(i) && i != subchannel) {
				DxScienceData scienceData;
				scienceData.requestType = REQ_SUBCHANNEL;
				scienceData.subchannel = i;
				sendSubchannelData(pol, hf, scienceData);
			}
		}
	}
}

/**
 * Send complex amplitudes for a subchannel.
 *
 * Description:\n
 * 	Sends one half-frame of complex amplitude data to the SSE.  This can
 * 	be either the single requested subchannel used by the SSE to display
 * 	a waterfall plot, or one of the ZX requested subchannels.  Returns the
 *	subchannel number.
 */
int32_t
Spectrometer::sendSubchannelData(Polarization pol, int32_t hf,
		const DxScienceData &scienceData)
{
	int32_t subchannel = -1;
	switch (scienceData.requestType) {
	case REQ_FREQ:
		// select subchannel by frequency
		subchannel  = channel->getSubchannel(scienceData.rfFreq);
		if (subchannel < 0 || subchannel >= subchannels) {
			if (!warningSent) {
				LogWarning(ERR_ISF, activity->getActivityId(),
						"freq = %.6lf, must be between %.6lf and %.6lf",
						scienceData.rfFreq, channel->getLowFreq(),
						channel->getHighFreq());
				warningSent = true;
			}
			return (subchannel);
		}
		break;
	case REQ_SUBCHANNEL:
		subchannel = scienceData.subchannel;
		if (subchannel < 0 || subchannel >= subchannels) {
			if (!warningSent) {
				LogWarning(ERR_ISF, activity->getActivityId(),
						"subchannel = %d, must be between %d and %d",
						scienceData.subchannel, 0, subchannels - 1);
				warningSent = true;
			}
			return (subchannel);
		}
		break;
	default:
		LogError(ERR_IRT, activity->getActivityId(),
				"activity %d, request type %d", activity->getActivityId(),
				scienceData.requestType);
		return (subchannel);
	}

	size_t len = sizeof(ComplexAmplitudeHeader) + sizeof(SubchannelCoef1KHz);
	MemBlk *blk = partitionSet->alloc(len);
	Assert(blk);
	ComplexAmplitudeHeader *caHdr
			= static_cast<ComplexAmplitudeHeader *> (blk->getData());
	SubchannelCoef1KHz *coefData = reinterpret_cast<SubchannelCoef1KHz *> (caHdr + 1);

	// build the header
	caHdr->rfCenterFreq = channel->getSubchannelCenterFreq(subchannel);
	caHdr->halfFrameNumber = hf;
	caHdr->activityId = activity->getActivityId();
	caHdr->hzPerSubchannel = MHZ_TO_HZ(channel->getSubchannelWidthMHz());
	caHdr->startSubchannelId = subchannel;
	caHdr->numberOfSubchannels = 1;
	caHdr->overSampling = channel->getSubchannelOversampling();
//	caHdr->res = RES_1KHZ;
	caHdr->pol = pol;

	// store the complex amplitude data
	SubchannelCoef1KHz *data
			= static_cast<SubchannelCoef1KHz *> (channel->getCdData(pol,
			subchannel, hf));
	*coefData = *data;

	// send the data
	Msg *msg = msgList->alloc(SEND_COMPLEX_AMPLITUDES,
			activity->getActivityId(), caHdr, len, blk);
	respQ->send(msg);
	return (subchannel);
}

/**
 * Send baseline statistics
 */
void
Spectrometer::sendBaselineStatistics()
{
	MemBlk *blk = partitionSet->alloc(sizeof(BaselineStatistics));
	Assert(blk);
	BaselineStatistics *stats
			= static_cast<BaselineStatistics *> (blk->getData());
	*stats = blStats;
	Msg *msg = msgList->alloc(SEND_BASELINE_STATISTICS,
			activity->getActivityId(), stats, sizeof(BaselineStatistics), blk);
	respQ->send(msg);
}
}
