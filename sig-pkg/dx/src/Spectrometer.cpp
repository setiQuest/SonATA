/*******************************************************************************

 File:    Spectrometer.cpp
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
// Spectrometer class (singleton)
//
//  This task does the spectrometry calculations
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/Spectrometer.cpp,v 1.12 2009/06/11 04:34:15 kes Exp $
//
#include <math.h>
#include <sstream>
#include <string.h>
#include <sys/time.h>
#include <DxOpsBitset.h>
#include "Dfb.h"
#include "Spectrometer.h"
#include "SignalIdGenerator.h"
#include "SmallTypes.h"

namespace dx {

Spectrometer *Spectrometer::instance = 0;

//#define DO_TIMING_TESTS
#ifdef DO_TIMING_TESTS
struct timeval tvE;
struct timeval tvL;
struct timezone tz;
#endif

Spectrometer *
Spectrometer::getInstance()
{
	static Lock l;
	l.lock();
	Assert(!instance);
	if (!instance)
		instance = new Spectrometer();
	l.unlock();
	return (instance);
}

Spectrometer::Spectrometer(): hanning(true), zeroDCBin(false),
		warningSent(false), firstHf(0), subchannels(0), startHf(0),
		baselineHf(0), halfFrame(0), resolutions(0),
		spectraHalfFrames(DEFAULT_SPECTRA_HALF_FRAMES), subchannelPulses(0),
		halfFramePulses(0), totalPulses(0), baselineReportingRate(0),
		waits(0), cdDataSize(0), cwDataSize(0), cdData(0), cwData(0),
		activity(0), channel(0), condition("spectrometer"),
		msgList(0), partitionSet(0), respQ(0), state(0)

{
	state = State::getInstance();
	Assert(state);
	msgList = MsgList::getInstance();
	Assert(msgList);
	partitionSet = PartitionSet::getInstance();
	Assert(partitionSet);
	SseOutputTask *sse = SseOutputTask::getInstance();
	Assert(sse);
	respQ = sse->getInputQueue();
	Assert(respQ);

	args = Args::getInstance();
	Assert(args);
	hanning = args->useHanning();
}

Spectrometer::~Spectrometer()
{
}

/**
 * Set up the spectrometer for an activity
 *
 * Description:\n
 * 	Resets the spectrometer to process the specified activity.  All internal
 * 	variables are initialized and any simulated signal is created.\n
 * Notes:\n
 * 	Since most state is retained in the activity and channel classes, there
 * 	is not too much to do here.
 */
Error
Spectrometer::setup(Activity *act)
{
	activity = act;
	Assert(activity);

	channel = activity->getChannel();
	Assert(channel);
	subchannels = channel->getUsableSubchannels();

	activity->getObsData(obs);
	baselineReportingRate = activity->getBaselineReportingRate();

	// clear the baseline arrays
	channel->clearBaselines();

	// allocate CD buffer
	size_t size = channel->getCdBytesPerSubchannelHalfFrame();
	if (cdData && cdDataSize < size) {
		fftwf_free(cdData);
		cdData = 0;
	}
	if (!cdData) {
		cdData = static_cast<ComplexPair *> (fftwf_malloc(size));
		cdDataSize = size;
	}
	Assert(cdData);

	if (!args->zxMode()) {
		// allocate CW buffer
		size = channel->getCwBytesPerSubchannel(RES_1HZ);
		if (cwData && cwDataSize < size) {
			fftwf_free(cwData);
			cwData = 0;
		}
		if (!cwData) {
			cwData = static_cast<uint64_t *> (fftwf_malloc(size));
			cwDataSize = size;
		}
		Assert(cwData);

		// clear the pulse map
		channel->clearPulseList();

		// set up the spectrometry library and allocate spectrum buffers
		setupSpectra();
	}

	// set the internal input half frame counter; this is used to
	// check that input always arrives in the correct order.
	startHf = halfFrame = -activity->getBaselineHalfFrames();
	// count the total number of frames which have been baselined.
	baselineHf = 0;
	return (0);
}

/**
 * Set up the spectrometry library for the activity.
 *
 * Description:\n
 * 	Initializes the spectrometry library for the specified set of resolutions
 * 	and whether they are overlapped, as well as the number of half frames of
 * 	data passed in a call and the number of samples per half frame.\n\n
 * Notes:\n
 * 	This version assumes overlap on all resolutions.\n
 * 	Spectra produces contiguous output spectra for a given resolution.
 */
void
Spectrometer::setupSpectra()
{
	// free any previously allocated buffers
	freeSpectraBufs();
	resolutions = activity->getResolutions();
	spectra.setup(activity->getResData(), resolutions, spectraHalfFrames,
			channel->getSamplesPerSubchannelHalfFrame(), resInfo);
	// now allocate the spectrum buffers for each resolution; note that
	// Spectra produces contiguous output spectra for a given resolution.
	for (int32_t i = 0; i < resolutions; ++i) {
		resSpectra.spectrum[i] = 0;
		int32_t size = resInfo[i].specLen * resInfo[i].nSpectra
				* sizeof(ComplexFloat32);
		resSpectra.buf[i] = static_cast<ComplexFloat32 *>
				(fftwf_malloc(size));
		Assert(resSpectra.buf[i]);
	}
}

/**
 * Process a half frame of data
 *
 * Description:\n
 * 	Called with a half frame of data from a worker task.  Performs
 * 	baselining followed by spectrometry to create all resolutions
 * 	specified by the activity parameters.  CD and CW data are stored
 * 	in buffers, while individual bins are thresholded to produce
 * 	pulses, which are stored in a pulse list.  Baselines are transmitted
 * 	to the SSE on a regular basis, and a single subchannel of CD data
 * 	is transmitted to the SSE every half frame.  It also keeps track
 * 	of the current half frame and stops data collection when it is
 * 	complete.\n
 * Notes:\n
 * 	Under rare conditions, when two or more worker tasks are running, it
 * 	might be possible for half frames to be delivered in the wrong
 * 	order.  Since it is essential that data be processed in sequential
 * 	order, a condition variable is used to block processing of data
 * 	which has not yet reached its turn.\n
 * 	The activity class (and the channel class within it) contain all
 * 	the information necessary for processing an observation.\n
 * 	If the half frame number is negative, it means we are baselining
 * 	the data, so it is not necessary to do actual spectrometry, but
 * 	to just fold the current data into the working baseline.
 *
 * @param	hf the half frame number of this buffer.
 * @param	bufPair	the buffer pair of L & R half frame data
 */
SpecState
Spectrometer::processHalfFrame(int32_t hf, BufPair *hfBuf)
{
	// if there's no activity, just release the buffer and
	if (!activity) {
		hfBuf->free();
		return (DCComplete);
	}
	// make sure the half frames match
	Assert(hf == halfFrame++);

	// add the half frame buffer to the list
	hfBufs.push_back(hfBuf);

	// do polarizations separately
	doPolarization(POL_RIGHTCIRCULAR, hf);
	doPolarization(POL_LEFTCIRCULAR, hf);
	++baselineHf;

	// if we are baselining, release the half frame buffer
	if (hf < 0) {
		hfBufs.pop_front();
		hfBuf->free();
		Assert(hfBufs.empty());
	}
	else if ((int32_t) hfBufs.size() >= spectraHalfFrames) {
		// release the number of half frames consumed by spectraLib
		freeHfBufs(spectraHalfFrames - 1);
		// we just created spectra, so count them
		for (int32_t i = 0; i < resolutions; ++i)
			resSpectra.spectrum[i] += resInfo[i].nSpectra;
	}

	// test for done just started baselining or data collection
	SpecState s;
	if (hf && hf == startHf)
		s = BLStarted;
	else if (hf == 0)
		s = DCStarted;
	else if (hf >= channel->getHalfFrames() - 1) {
		channel->stopDataCollection();
		freeHfBufs(spectraHalfFrames);
		Assert(hfBufs.empty());
		s = DCComplete;
	}
	else
		s = NoChange;
	channel->incrementHalfFrame();
	return (s);
}

/**
 * Stop the spectrometer.
 *
 * Description:\n
 * 	Terminates spectrometry, releasing all data buffers.
 */
void
Spectrometer::stopSpectrometry(Activity *act)
{
	// make sure we're stopping the correct activity
	if (!activity)
		return;
	if (!act || act == activity) {
		channel->stopDataCollection();
	//	freeSpectraBufs();
		freeHfBufs(spectraHalfFrames);
		activity = 0;
	}
}

void
Spectrometer::freeSpectraBufs()
{
	resSpectra.init();
	resolutions = 0;
}

/**
 * Free all completed half frame buffers.
 *
 * Description:\n
 * 	When spectrometry has been performed on a set of half frames, all
 * 	half frame buffers except the last one can be released into the
 * 	buffer pool.
 *
 * @param		n number of half frame buffers to free.
 */
void
Spectrometer::freeHfBufs(int32_t n)
{
	for (int32_t i = 0; i < n && !hfBufs.empty(); ++i) {
		BufPair *buf = hfBufs.front();
		hfBufs.pop_front();
		buf->free();
	}
}
/**
 * Do spectrometry for a single polarization
 *
 * Description:\n
 * 	Perform spectrometry for all resolutions for all subchannels for
 * 	a single polarization.  Input data is a buffer containing a
 * 	half-frame of corner-turned data for each subchannel.
 *
 * @param		pol polarization.
 * @param		hf half frame #
 * @param		hfBuf the buffer pair of half frame data.
 */
void
Spectrometer::doPolarization(Polarization pol, int32_t hf)
{
	int32_t end = channel->getUsableSubchannels();
#ifdef notdef
	char msg[dx::MAX_STR_LEN];
	sprintf(msg,
			"doPol: actId: %d channel %d, hf %d ",
			activity->getActivityId(), obs.channel, hf);
//	Debug(DEBUG_SPECTROMETRY, halfFrame, msg);
#endif
	// use the correct spectrum library
	halfFramePulses = 0;

	// process each subchannel
	for (int32_t i = 0; i < end; ++i)
		processSubchannel(pol, i, hf);
	sendScienceData(pol, hf);
}

/**
 * Process a single subchannel for one polarization and one half frame.
 *
 * Description:\n
 * 	Performs baselining and spectrometry for a single subchannel.  The
 * 	output spectrum data is stored in its final destination.
 *
 * @param		pol polarization.
 * @param		subchannel.
 * @param		hf half frame.
 */
void
Spectrometer::processSubchannel(Polarization pol, int32_t subchannel,
		int32_t hf)
{
	bool masked = activity->isSubchannelMasked(subchannel);

	// get the most recent half frame buffer; it is at the end of the
	// half frame buffer list
	BufPair *buf = hfBufs.back();
	ComplexFloat32 *hfData = static_cast<ComplexFloat32 *>
			(channel->getHfData(pol, subchannel, buf));

	// compute the baseline
	if (!masked)
		computeBaseline(pol, subchannel, hfData, obs.baselineWeighting);

	// if we're still baselining, return
	if (hf < 0)
		return;

	// the half frame buffers contain the baselined data with any signals
	// inserted.  Now output the CD data.
	if (obs.cdOutputOption == normal) {
		if (masked)
			zeroCdData();
		else
			computeCdData(hfData);
	}
	else if (obs.cdOutputOption == tagged_data)
		loadCdPattern(pol, subchannel, hf);
	storeCdData(pol, subchannel, hf);

	// do spectrometer processing only if this a DX, not a ZX
	if (!args->zxMode()) {
		// now create the spectra if we have enough half frames
		if ((int32_t) hfBufs.size() < spectraHalfFrames)
			return;

		ComplexFloat32 *hfD[spectraHalfFrames];
		for (int32_t i = 0; i < spectraHalfFrames; ++i)
			hfD[i] = static_cast<ComplexFloat32 *> (channel->getHfData(pol,
					subchannel, hfBufs[i]));
		spectra.computeSpectra(hfD, resSpectra.buf);

		// do all resolutions
		// The data must be rearranged to be in increasing frequency order,
		// because it is returned from Spectra in ordinary FFT order (index 0
		// = DC, index n-1 = -1.  We want index 0 = -n/2, index n/2 = 0,
		// index n-1 = n/2-1.  So DC is in the middle
		for (int32_t i = 0; i < resolutions; ++i) {
			Resolution res = resInfo[i].res;
			int32_t specLen = resInfo[i].specLen;
			int32_t nSpectra = resInfo[i].nSpectra;

#ifdef notdef
			// handle the spectra individually; the number of spectra depends
			// upon the resolution and whether the spectra are overlapped;
			// the length of each spectrum depends upon the resolution.  We
			// also remove the unused bins at this time
			int32_t h = channel->getTotalBinsPerSubchannel((Resolution) i) / 2;
#ifdef notdef
			int32_t d = channel->getTotalBinsPerSubchannel((Resolution) i) - h;
#endif
			size_t len = h * sizeof(ComplexFloat32);
			ComplexFloat32 tmp[TOTAL_BINS_PER_SUBCHANNEL_1HZ];
			ComplexFloat32 *data = resSpectra.buf[i];
			for (int32_t j = 0; j < nSpectra; ++j) {
				memcpy(tmp, data, len);
				memcpy(data, data + h, len);
				memcpy(data + h, tmp, len);
				data += specLen;
			}
#endif
			for (int32_t j = 0; j < nSpectra; ++j) {
				ComplexFloat32 *data = resSpectra.buf[i] + j * specLen;
				// do pulse thresholding for the resolution
				if (!masked) {
					storePulseData(pol, res, subchannel, resSpectra.spectrum[i] + j,
							obs.pulseThreshold, data);
				}
				// test for CW resolution
				if (res == obs.cwResolution) {
					if (obs.cwOutputOption == normal) {
						if (masked)
							zeroCwData();
						else
							computeCwData(res, data);
					}
					else if (obs.cwOutputOption == tagged_data)
						loadCwPattern(pol, res, subchannel,
								resSpectra.spectrum[i] + j);
					storeCwData(pol, res, subchannel, resSpectra.spectrum[i] + j);
				}
			}
		}
	}
}

/**
 * Compute the baseline and apply to the data.
 *
 * Description:\n
 * 	For each subchannel and half frame, the average power in a bin is
 * 	computed (using all the bins in the half frame for that subchannel).
 * 	The reciprocal of this value is combined with the existing baseline
 * 	value for the subchannel using an aging algorithm to produce a new
 * 	baseline.\n\n
 * Notes:\n
 * 	This version computes a new baseline using the latest half frame,
 * 	then applies the old baseline to the half frame.  If it is desired
 * 	to compute the new baseline and apply it to the current
 * 	half frame, move the second line from the loop and add a second
 * 	loop after the baseline
 */
void
Spectrometer::computeBaseline(Polarization pol, int32_t subchannel,
		ComplexFloat32 *hfData, float32_t weighting)
{
	// compute the number of initial half frames to fully prime the
	// baseline
	int32_t k = 1 / (1.0 - weighting);

	float32_t w = weighting;
	if (baselineHf < k)
		w = 1 - 1.0 / (baselineHf + 1);
	int32_t n = channel->getSamplesPerSubchannelHalfFrame();

	float32_t bl, power = 0;
	float32_t *baseline = channel->getBlData(pol);
	bl = baseline[subchannel];
	for (int32_t i = 0; i < n; ++i)
		power += std::norm(hfData[i]);
	// we've now computed the power in the subchannel; create a new baseline
	power *= BASELINE_FACTOR;
	float32_t hfBl = power ? sqrt(n / power) : 0;
	baseline[subchannel] = w * bl + (1 - w) * hfBl;
	for (int32_t i = 0; i < n; ++i)
		hfData[i] *= baseline[subchannel];
}

/**
 * Compute CD data.
 *
 * Description:\n
 * 	Converts subchannel data from complex floating point to complex
 * 	4-bit integer format.  Data which exceeds the maximum range are
 * 	saturated to the maximum range.\n
 *
 * @param		data pointer to floating-point subchannel data.  The data has
 * 				been baselined.
 */
void
Spectrometer::computeCdData(ComplexFloat32 *data)
{
	int32_t n = channel->getSamplesPerSubchannelHalfFrame();
#if FLOAT4_ARCHIVE
	for (int32_t i = 0; i < n; ++i) {
		ComplexFloat4 d(data[i]);
		cdData[i] = (ComplexPair) d;
	}
#else
	const int32_t max = MAX_CD_VAL, min = -MAX_CD_VAL;

	for (int32_t i = 0; i < n; ++i) {
		int32_t re = (int32_t) rint(data[i].real());
		int32_t im = (int32_t) rint(data[i].imag());
		if (re > max)
			re = max;
		if (re < min)
			re = min;
		if (im > max)
			im = max;
		if (im < min)
			im = min;
		cdData[i].pair = (re << 4) | (im & 0xf);
	}
#endif
}

/**
 ** zeroCDData: Set the data to zero for subchannels that are masked.
 */
void
Spectrometer::zeroCdData()
{
	memset(cdData, 0, channel->getCdBytesPerSubchannelHalfFrame());
}

/**
 * Load a CD test pattern in the output buffer.
 *
 * Description:\n
 ** build a test pattern in the CD output buffer
 ** bits 31-22 Spectrum
 ** bits 21-10 Subchannel
 ** bits 9-0 subchannel spectrum
 */
void Spectrometer::loadCdPattern(Polarization pol, int32_t subchannel,
		int32_t hf)
{
	int32_t samples = channel->getSamplesPerSubchannelHalfFrame();
	uint32_t *p = (uint32_t *) cdData;

	uint32_t seed = (halfFrame << 8) | ((subchannel) << 16);

	for (int32_t i = 0, j = 0; i < samples; i += 4)
		p[j++] = seed | (i / 4);
}

/**
 * Store the CD data for a single subchannel.
 *
 * Description:\n
 * 	Stores the CD data for a single subchannel.  Since all CD data is buffered
 * 	in memory, the data is corner-turned into its final location in the
 * 	buffer.
 *
 * @param		subchannel subchannel #
 * @param		hf half frame #
 * @param		pol polarization
 */
void
Spectrometer::storeCdData(Polarization pol, int32_t subchannel, int32_t hf)
{
	void *buf = channel->getCdData(pol, subchannel, hf);
	memcpy(buf, cdData, channel->getCdBytesPerSubchannelHalfFrame());
}

/**
 * Zero the CW data.
 *
 * Description:\n
 * 	Sets the CW data for a masked subchannel to zero.  The data in the
 * 	final destination buffer is zeroed.
 */
void
Spectrometer::zeroCwData()
{
	memset(cwData, 0, channel->getCwBytesPerSubchannel(RES_1HZ));
}

/**
 * Build a test pattern in the CW output buffer.
 *
 * Description:\n
 * 	Builds a test pattern in the CW buffer.  The pattern consists of
 * 	64-bit words: 16 bits of subchannel, 16 bits of spectrum, 32 bits of
 * 	iteration.
 *
 * @param		subchannel
 * @param		frame
 */
void
Spectrometer::loadCwPattern(Polarization pol, Resolution res,
		int32_t subchannel, int32_t spectrum)
{
	int32_t spectrumBins = channel->getCwBinsPerSubchannel(res);

	// get the address of the CW buffer
	uint64_t *buf = static_cast<uint64_t *>
			(channel->getCwData(pol, res, subchannel, spectrum));

	// do 32 bins (64 bits) at a time
	int32_t j = 0;

	uint64_t seed = ((uint64_t) subchannel << 48)
			| ((uint64_t) spectrum << 32);
	for (int32_t i = 0; i < spectrumBins; i += sizeof(uint64_t))
		buf[j++] = seed + i;
}

/**
 * Compute the CW data and pack it into the temporary buffer.
 *
 * Description:\n
 * 	Converts the spectrum data for a single spectrum at a single
 * 	resolution to CW format, which is 2-bit power packed 4 bins
 * per byte, then stores the data in the CW temp buffer.\n\n
 * Notes:\n
 * 	We perform spectrometry for all subchannels, even those that
 * 	are masked
 *
 * @param		pol polarization.
 * @param		res resolution.
 * @param		subchannel subchannel of the spectra
 * @param		spectrum spectrum #.
 * @param		data complex spectrum data.
 */
void
Spectrometer::computeCwData(Resolution res, const ComplexFloat32 *data)
{
	int32_t totalBins = channel->getTotalCwBinsPerSubchannel(res);
	int32_t usableBins = channel->getCwBinsPerSubchannel(res);
	int32_t hd = (totalBins - usableBins) / 2;
	int32_t start = hd;
	int32_t end = totalBins - hd;

	// do 32 bins (64 bits) at a time
	int32_t bins = sizeof(uint64_t) * CWD_BINS_PER_BYTE;
	uint32_t max = (1 << CWD_BITS_PER_BIN) - 1;
	int32_t k = 0;
	ComplexFloat32 p[3];
	p[0] = data[start-1];
	p[1] = data[start];
	// scale by the Hanning power gain, which is .375
	float32_t hanningScale = sqrt(8.0/3.0) / 2;
	for (int32_t i = start; i < end; i += bins) {
		const ComplexFloat32 *d = &data[i];
		uint64_t val = 0;
		for (int32_t j = 0; j < bins; ++j) {
			p[2] = d[j+1];
			ComplexFloat32 bin = p[1];
			if (hanning) {
				ComplexFloat32 adj = p[0] + p[2];
				adj *= .5;
				bin += adj;
				bin *= hanningScale;
			}
			uint64_t power = (uint32_t) std::norm(bin);
			if (power > max)
				power = max;
			val |= (power << (j * CWD_BITS_PER_BIN));
			p[0] = p[1];
			p[1] = p[2];
		}
		cwData[k++] = val;
	}
}

/**
 * Store the CW data in the destination buffer.
 *
 * Description:\n
 * 	Copies the CW data from the temp buffer to its final destination
 * 	in the CW buffer.\n\n
 *
 * Notes:\n
 * 	The data has already been converted to packed 2-bit power.
 *
 * @param		pol polarization.
 * @param		res resolution.
 * @param		subchannel.
 * @param		spectrum spectrum #.
 */
void
Spectrometer::storeCwData(Polarization pol, Resolution res, int32_t subchannel,
		int32_t spectrum)
{
	void *buf = channel->getCwData(pol, res, subchannel, spectrum);
	memcpy(buf, cwData, channel->getCwBytesPerSubchannel(res));
}

/**
 * Threshold and store the pulse data.
 *
 * Description:\n
 * 	Thresholds the data for a spectrum, adding all over-threshold bins
 * 	as pulses to the pulse vector.\n\n
 * Notes:\n
 * 	At this point, the pulses are stored in a simple vector; in the
 * 	pulse detector, they will be put in a map and polarizations
 * 	combined.
 *
 * @param		pol polarization.
 * @param		res resolution.
 * @param		subchannel.
 * @param		spectrum spectrum #.
 * @param		data spectrum data.
 */
void
Spectrometer::storePulseData(Polarization pol, Resolution res,
		int32_t subchannel, int32_t spectrum,
		float32_t threshold, const ComplexFloat32 *data)
{
	int32_t bins = channel->getUsableBinsPerSubchannel(res);
	int32_t start = (channel->getTotalBinsPerSubchannel(res) - bins) / 2;
	int32_t end = start + bins;
	int32_t subchannelPulses = 0;

	for (int32_t i = start; i < end; ++i) {
		float32_t power = std::norm(data[i]);
		if (power > threshold) {
			// got a pulse
			++subchannelPulses;
			++halfFramePulses;
			++totalPulses;
			if (subchannelPulses
					<= (int32_t) obs.maxPulsesPerSubchannelPerHalfFrame
					&& halfFramePulses <= (int32_t) obs.maxPulsesPerHalfFrame) {
				int32_t bin = subchannel * bins + (i - start);
				dx::Pulse pulse(res, bin, spectrum, pol, power, 0.0);
				channel->addPulse(pulse);
			}
		}
	}
}

}
