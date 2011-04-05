/*******************************************************************************

 File:    Channel.cpp
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
// DX channel class
//
//
#include <sys/time.h>
#include "System.h"
#include "Activity.h"
#include "Channel.h"
#include "DxErr.h"
#include "State.h"

namespace dx {

/**
* Channel class.
*
* When the channel is created, the activity parameters are used to
* compute the parameters for the data collection buffers (CD, CW,
* baseline).  In addition, the transfer specifications for the
* corner turn into the buffers is also computed.  Several dual-pol
* buffers are allocated: input samples buffers, half frame buffers
* for subchannelized data, CD and CW.  The latter are large enough
* to hold the entire output of the activity, and data will be
* stored in them (by the spectrometer) in final corner-turned
* order.  The input sample and half frame buffers can be released
* as soon as data collection is complete.  The CW buffers can be
* released as soon as
*/
Channel::Channel(State *state_, Activity *activity_): initialized(false), swap(false),
		singlePol(false), armed(false), abortCollection(false),
		inactivePol(ATADataPacketHeader::NONE),
		rPol(ATADataPacketHeader::RCIRC), lPol(ATADataPacketHeader::LCIRC),
		subchannels(0), schedules(0), flushes(0), dones(0), consumed(0),
		threshold(0), inputHalfFrame(0), halfFrame(0), startSeq(0), curSeq(0),
		sampleCnt(0), freq(0), pol(POL_UNINIT), activity(activity_),
		left(0), right(0), blBuf(0), newBlBuf(0), cdBuf(0), cwBuf(0),
		detectionBuf(0), hfBufList(0), channelState(DX_ACT_NONE),
		cLock("channel"), args(0), pktList(0),
		msgList(0), partitionSet(0), workQ(0), state(state_)
{
}

#ifdef notdef
Channel::Channel(Activity *activity_, int chan_, int32_t halfFrames_,
		float64_t freq_, float64_t bandwidth_, int thresh_):
		activity(activity_),
		halfFrames(halfFrames_),
		freq(freq_), bandwidth(bandwidth_), threshold(thresh_),
		channelState(DX_ACT_INIT)
{
	init();
	setup(activity_, chan_, halfFrames_, freq_, bandwidth_, thresh_);
}
#endif

Channel::~Channel()
{
//	delete rCTBuf;
//	delete lCTBuf;
//	delete right;
//	delete left;
}

/**
 * Set the filter parameters
 *
 * Description:\n
 * 	Allows specification of the actual filter parameters, including
 * 	coefficients, to be used by the DFB.\n
 * Notes:\n
 * 	A filter file can be read with the ReadFilter class in the SonATA
 * 	library.  This method loads the basic filter parameters, including the
 * 	coefficients.  Actual filter length can be set with Dfb::setup.
 */
void
Channel::setFilter(const FilterSpec& filter_)
{
	filter = filter_;

	dfb.setCoeff(filter.coeff, filter.fftLen, filter.foldings);
}

/**
 * Set up the channel for processing
 *
 * Description:\n
 * 	Resets all initialization values to prepare the channel for use
 * 	in a new activity.\n\m
 * Notes:\n
 * 	Each activity contains a channel, which must be initialized before
 * 	use.  This should be done automatically when the activity is
 * 	allocated.
 */
void
Channel::setup(const DxActivityParameters& params, Polarization pol_)
{
	Assert(activity);

	lock();

	// perform initialization if necessary
	init();

	// initialize internal variables
//	channelState = DX_ACT_INIT;
	sampleCnt = 0;
	inputHalfFrame = halfFrame = -params.baselineInitAccumHalfFrames;
//	halfFrames = activity->getHalfFrames();
	curSeq = 0;

	// allocate input buffers
	Assert(!left);
	Assert(!right);
	left = state->allocInputBuf();
	Assert(left);
	right = state->allocInputBuf();
	Assert(right);
	left->reset();
	right->reset();

	cdBuf->initialize();
	cwBuf->initialize();
	blBuf->initialize();
	newBlBuf->initialize();
	stats.netStats.reset();

	// initialize the channel structure
	channelSpec.chan = activity->getChannelNum();
//	channelSpec.chan = 0;
	channelSpec.src = args->getSrc();
	channelSpec.pol = pol_;
	channelSpec.freq = activity->getSkyFreq();
	channelSpec.bandwidth = args->getChannelBandwidth();
	channelSpec.oversampling = args->getChannelOversampling();

	// set the polarization
	pol = pol_;
	singlePol = false;
	switch (pol) {
	case POL_BOTH:
		rPol = ATADataPacketHeader::RCIRC;
		lPol = ATADataPacketHeader::LCIRC;
		inactivePol = ATADataPacketHeader::NONE;
		break;
	case POL_BOTHLINEAR:
		rPol = ATADataPacketHeader::XLINEAR;
		lPol = ATADataPacketHeader::YLINEAR;
		inactivePol = ATADataPacketHeader::NONE;
		break;
	case POL_RIGHTCIRCULAR:
		rPol = ATADataPacketHeader::RCIRC;
		lPol = ATADataPacketHeader::NONE;
		inactivePol = ATADataPacketHeader::LCIRC;
		singlePol = true;
		break;
	case POL_LEFTCIRCULAR:
		rPol = ATADataPacketHeader::NONE;
		lPol = ATADataPacketHeader::LCIRC;
		inactivePol = ATADataPacketHeader::RCIRC;
		singlePol = true;
		break;
	case POL_XLINEAR:
		rPol = ATADataPacketHeader::XLINEAR;
		lPol = ATADataPacketHeader::NONE;
		inactivePol = ATADataPacketHeader::YLINEAR;
		singlePol = true;
		break;
	case POL_YLINEAR:
		rPol = ATADataPacketHeader::NONE;
		lPol = ATADataPacketHeader::YLINEAR;
		inactivePol = ATADataPacketHeader::XLINEAR;
		singlePol = true;
		break;
	default:
		Fatal(ERR_IPT);
		break;
	}

	// set up the DFB
	dfb.setup(getTotalSubchannels(),
			getTotalSubchannels() * getSubchannelOversampling(),
			getFoldings(), getSamplesPerSubchannelHalfFrame());
	dfb.getInfo(&dfbInfo);
	allocPulseList(params);

	// set the number of samples required for DFB processing
//	threshold = dfbInfo.dataLen;
	int32_t overlap = (int32_t) (getTotalSubchannels()
			* getSubchannelOversampling());
	threshold = dfb::Dfb::getThreshold(getTotalSubchannels(),
			getFoldings(), overlap, getSamplesPerSubchannelHalfFrame());
	consumed = (getTotalSubchannels() - overlap)
			* getSamplesPerSubchannelHalfFrame();

	abortCollection = false;
	data.reset();

	unlock();
}

/**
 * Reserve pulse list space for the expected number of pulses.
 *
 * Description:\n
 * 	Computes the expected number of pulses for the activity based
 * 	on # of frames, # of resolutions requested and pulse threshold,
 * 	then reserves space for that many pulses in the vector.\n\n
 * Notes:\n
 * 	This function will be a NOP except for the first time the
 * 	activity is used and when a longer activity length is specified.\n
 * 	It is important that the detector not have to reallocate vector
 * 	space during data collection where time is critical.
 */
void
Channel::allocPulseList(const DxActivityParameters& params)
{
	double totalBins = 0;
	for (int32_t i = 0; i < MAX_RESOLUTIONS; ++i) {
		if (params.requestPulseResolution[i] == SSE_TRUE)
			totalBins += getUsableBinsPerSpectrum((Resolution) i)
					* getSpectra((Resolution) i);
	}
	double threshold = params.pd[RES_1HZ].pulseThreshold;
	double pulses = totalBins * exp(-threshold);
	// two polarizations and a safety factor
	pulses *= 2 * PULSE_SAFETY_FACTOR;
	pulseList.reserve((int32_t) pulses);
}

/**
 * Initialize the channel structure.
 *
 * Description:\n
 * 	This is called exactly once by the parent activity the first time
 * 	the activity is used.
 * 	A channel is encapsulated inside an activity, so it is created when
 *	the activity is created at startup.  The data buffers (input, cw, cd
 *  and baseline) are created by a subsequent init call by the parent
 * 	activity.  The buffers are allocated for the maximum activity length
 * 	specified at startup and are retained by the channel throughout the
 * 	execution of the program.
 */
void
Channel::init()
{
	// if we've already done initialization, nothing to do
	if (initialized)
		return;

	args = Args::getInstance();
	Assert(args);
	msgList = MsgList::getInstance();
	Assert(msgList);
	partitionSet = PartitionSet::getInstance();
	Assert(partitionSet);
	pktList = ChannelPacketList::getInstance(CHANNEL_PACKETS);
	Assert(pktList);
//	state = State::getInstance();
//	Assert(state);
	workQ = WorkQ::getInstance();
	Assert(workQ);

#ifdef notdef
	// set the parent activity
	activity = parent_;

	swap = args->swapInputs();
#endif

	// allocate all buffers except the detection buffer, which is shared
	// between activities

	// set up the DFB
	if (args->useCustomFilter()) {
		const FilterSpec& filter = args->getFilter();
		dfb.setCoeff(filter.coeff, filter.fftLen, filter.foldings);
	}
	dfb.setup(getTotalSubchannels(),
			getTotalSubchannels() * getSubchannelOversampling(),
			getFoldings(), getSamplesPerSubchannelHalfFrame());
	dfb.getInfo(&dfbInfo);

#ifdef notdef
	// allocate the input buffers
	size_t size = BUF_COUNT * dfbInfo.fftLen * dfbInfo.samplesPerChan;
	left = new InputBuffer(size, sizeof(ComplexInt16));
	if (!left)
		Fatal(ERR_MAF);
	right = new InputBuffer(size, sizeof(ComplexInt16));
	if (!right)
		Fatal(ERR_MAF);
#endif
	// allocate the cd buffers for the channel
	size_t size = getCdSamplesPerSubchannelHalfFrame() * state->getCdBytesPerSample()
			* getUsableSubchannels() * getMaxHalfFrames();
	cdBuf = new BufPair("cdBuf", size, false, NULL);
	if (!cdBuf)
		Fatal(ERR_NBA);

	// allocate the cw buffers for the channel
	size = getCwBytesPerSpectrum(RES_1HZ) * getSpectraPerFrame(RES_1HZ)
			* getMaxFrames();
	cwBuf = new BufPair("cwBuf", size, false, NULL);
	if (!cwBuf)
		Fatal(ERR_NBA);

	// allocate the baseline buffers
	size = getBlBytesPerHalfFrame();
	blBuf = new BufPair("blBuf", size, true, NULL);
	if (!blBuf)
		Fatal(ERR_NBA);
	newBlBuf = new BufPair("newBlBuf", size, true, NULL);
	if (!newBlBuf)
		Fatal(ERR_NBA);
	initialized = true;
}

/**
 * Functions which reference the State class.
 *
 * These are values which do not change during execution.
 */
int32_t
Channel::getMaxFrames()
{
	return (state->getMaxFrames());
}

int32_t
Channel::getMaxHalfFrames()
{
	return (state->getMaxHalfFrames());
}

int32_t
Channel::getSamplesPerSubchannelHalfFrame()
{
	return (state->getSamplesPerSubchannelHalfFrame());
}

int32_t
Channel::getBytesPerSubchannelHalfFrame() {
	return (state->getBytesPerSubchannelHalfFrame());
}

int32_t
Channel::getTotalSubchannels()
{
	return (state->getTotalSubchannels());
}

int32_t
Channel::getUsableSubchannels()
{
	return (state->getUsableSubchannels());
}

float64_t
Channel::getSubchannelWidthMHz()
{
	return (state->getSubchannelWidthMHz());
}

float64_t
Channel::getSubchannelRateMHz()
{
	return (state->getSubchannelRateMHz());
}

int32_t
Channel::getFoldings()
{
	return (state->getFoldings());
}

float64_t
Channel::getSubchannelOversampling()
{
	return (state->getSubchannelOversampling());
}

int32_t
Channel::getSubchannelsPerArchiveChannel()
{
	return (state->getSubchannelsPerArchiveChannel());
}

int32_t
Channel::getTotalBinsPerSubchannel(Resolution res) {
	return (state->getTotalBinsPerSubchannel(res));
}

int32_t
Channel::getUsableBinsPerSubchannel(Resolution res) {
	return (state->getUsableBinsPerSubchannel(res));
}

int32_t
Channel::getUsableBinsPerSpectrum(Resolution res) {
	return (state->getUsableBinsPerSpectrum(res));
}

int32_t
Channel::getSpectraPerFrame(Resolution res)
{
	return (activity->getSpectraPerFrame(res));
}

int32_t
Channel::getSpectra(Resolution res)
{
	return (activity->getSpectra(res));
}

#ifdef notdef
/**
* Set the activity start time.
*
* Description:\n
*	Sets the start time.  The state must be set to tuned when this function
*	is called.
*/
Error
Channel::setStartTime(const NssDate& start_, DxActivityState state_)
{
	Error err = 0;

	lock();
	if (getState() == DX_ACT_INIT) {
		startTime = start_;
		setState(state_);
	}
	else
		err = ERR_IAS;
	unlock();
	return (err);
}
#endif

/**
 * Stop data collection.
 *
 * Description:\n
 * 	Terminate data collection, releasing all internal resources.\n\n
 *
 * Notes:\n
 * 	Called by a worker task when the spectrometer indicates that all
 * 	data has been collected.
 */
void
Channel::stopDataCollection()
{
	lock();
//	if ((channelState == DX_ACT_RUN_BASE_ACCUM
//	|| channelState == DX_ACT_RUN_DC)) {
	if ((getState() == DX_ACT_RUN_BASE_ACCUM
			|| getState() == DX_ACT_RUN_DC)) {
		clearPacketQueues();
		// release the input buffers
		if (getState() == DX_ACT_RUN_BASE_ACCUM)
			setState(DX_ACT_BASE_ACCUM_COMPLETE);
		else
			setState(DX_ACT_DC_COMPLETE);
		armed = false;
		clearInputBuffers();
	}
	unlock();
	if (abs(data.err) > abs(data.maxErr)) {
		data.maxErr = data.err;
		cout << "Max seq err: " << data.maxErr << endl;
	}
}

/**
 * Release all pending packets in the queue
 */
void
Channel::clearPacketQueues()
{
	// release all pending packets in the queue
	ChannelPacket *p;
	while (!r.empty()) {
		p = r.front();
		r.pop();
		pktList->free(p);
	}
	Assert(r.empty());
	while (!l.empty()) {
		p = l.front();
		l.pop();
		pktList->free(p);
	}
	Assert(l.empty());
}

int32_t
Channel::getActivityId()
{
	return (activity->getActivityId());
}

void
Channel::setState(DxActivityState newState)
{
	activity->setState(newState);
}

DxActivityState
Channel::getState()
{
	return (activity->getState());
}
int32_t
Channel::getFrames()
{
	return (activity->getFrames());
}

int32_t
Channel::getHalfFrames()
{
	return (activity->getHalfFrames());
}

float64_t
Channel::getLowFreq()
{
	float64_t freq = activity->getSkyFreq() - (getChannelWidthMHz()
			+ getSubchannelWidthMHz()) / 2;
	return (freq);
}

float64_t
Channel::getHighFreq()
{
	float64_t freq = activity->getSkyFreq() + (getChannelWidthMHz()
			- getSubchannelWidthMHz()) / 2;
	return (freq);
}

float64_t
Channel::getSubchannelCenterFreq(int32_t subchannel)
{
	float64_t freq = getLowFreq()
			+ (subchannel + 0.5) * getSubchannelWidthMHz();
	return (freq);
}

//
// getSubchannel: compute the subchannel containing the specified
//		frequency
//
// Notes:
//		Computes the subchannel containing the specified frequency.
//		The subchannel will be computed even if it lies outside
//		the frequency range of the DX.
//		This is the subchannel relative to the assigned bandwidth
//		of the DX, which may be different from the absolute
//		subchannel, which is based on the maximum bandwidth of the
//		DX
//
int32_t
Channel::getSubchannel(float64_t freq_)
{
	int32_t subchannel;

	subchannel = static_cast<int32_t> ((freq_ - getLowFreq())
			/ getSubchannelWidthMHz());
	return (subchannel);
}

/**
 * Half frame buffer calls.
 *
 * A half frame buffer contains the filtered data for all subchannels
 * of a single call to the DFB, which consists of a full half frame.
 */
BufPair *
Channel::allocHfBuf()
{
	return (state->allocHfBuf());
}

void
Channel::freeHfBuf(BufPair *hfBuf)
{
	state->freeHfBuf(hfBuf);
}

/**
 * Compute the offset of a subchannel in a half frame buffer.
 *
 * Description:\n
 * 	Computes the sample index of the beginning of a given subchannel
 * 	in the half frame buffer.
 */
uint32_t
Channel::getHfOfs(int32_t subchannel)
{
	return (subchannel * state->getSamplesPerSubchannelHalfFrame());
}

/**
 * Compute the address of a subchannel of data for a half frame buffer.
 *
 * Description:\n
 * 	Returns the address of single subchannel of data in a half frame
 * 	buffer.
 *
 * @param		pol polarization.
 * @param		subchannel.
 * @param		buf half frame buffer.
 */
ComplexFloat32 *
Channel::getHfData(Polarization pol, int32_t subchannel, BufPair *hfBuf)
{
	uint32_t ofs = subchannel * state->getBytesPerSubchannelHalfFrame();
	void *data = hfBuf->getBufData(getPolCode(pol));
	ComplexFloat32 *buf = reinterpret_cast<ComplexFloat32 *>
			(static_cast<uint8_t *> (data) + ofs);
	return (buf);
}

/**
 * Confirmation data buffer calls.
 *
 * The confirmation buffer contains all subchannel data for the entire
 * observation.  It is used in coherent qualification of CW signals and
 * in confirmation or rejection of signals from other detectors.
 */
Buffer *
Channel::getCdBuf(Polarization pol)
{
	return (cdBuf->getBuf(getPolCode(pol)));
}

/**
 * Compute the address of a CD subchannel/half frame.
 *
 * Description:\n
 * 	Returns the address of a single half frame of CD data for a single
 * 	subchannel.  This is used when storing the CD data during spectrometry, when the CD data
 * 	is corner-turned, or during confirmation, when the data is retrieved.\n\n
 * Notes:\n
 * 	During confirmation, the data for a subchannel is retrieved as a
 * 	single contiguous block; the beginning of this block can be obtained
 * 	by calling this function with a half frame of 0.
 *
 * @param		pol polarization code for the buffer.
 * @param		subchannel subchannel.
 * @param		hf half frame.
 */
void *
Channel::getCdData(Polarization pol, int32_t subchannel, int32_t hf)
{
	uint32_t ofs = subchannel * getCdBytesPerSubchannel();
	ofs += hf * getCdBytesPerSubchannelHalfFrame();
	uint8_t *data = static_cast<uint8_t *> (cdBuf->getBufData(getPolCode(pol)));
	void *buf = data + ofs;
	return (buf);
}

/**
 * Return the number of samples in one half frame for one subchannel
 */
int32_t
Channel::getCdSamplesPerSubchannelHalfFrame()
{
		return (state->getSamplesPerSubchannelHalfFrame());
}

/**
 * Return the number of bytes for one subchannel for the en
 */
int32_t
Channel::getCdBytesPerSubchannelHalfFrame()
{
		return ((int32_t) (getCdSamplesPerSubchannelHalfFrame()
				* state->getCdBytesPerSample()));
}

int32_t
Channel::getCdSamplesPerSubchannel()
{
	return (getCdSamplesPerSubchannelHalfFrame() * getHalfFrames());
}

int32_t
Channel::getCdBytesPerSubchannel()
{
	return ((int32_t) (getCdSamplesPerSubchannel()
			* state->getCdBytesPerSample()));
}

int32_t
Channel::getCdStridePerSubchannel()
{
	return ((int32_t) getCdSamplesPerSubchannel());
}

/**
 * CW buffer calls.
 *
 * The CW buffer contains power data for a single resolution.  It is used
 * in DADD detection.
 */
Buffer *
Channel::getCwBuf(Polarization pol)
{
	return (cwBuf->getBuf(getPolCode(pol)));
}

/**>
 * Compute the address of a CW spectrum.
 *
 * Description:\n
 * 	Returns the address of a spectrum of CW data for a specific polarization,
 * 	subchannel and half frame.  This is used when storing
 * 	the CW data during spectrometry.  The spectra library computes multiple
 * 	spectra at a time, so this function effectively provides the starting
 * 	address of a block of spectra.\n\n
 * Notes:\n
 * 	CW spectra are stored with the entire spectrum, from subchannel 0 to
 * 	subchannel N-1 for a given half frame, contiguous in memory as one long
 * 	row.\n
 * 	CW data is 2-bit power, packed 4 bins per byte.
 *
 * @param		pol polarization code for the buffer.
 * @param		subchan subchannel.
 * @param		hf half frame.
 */
void *
Channel::getCwData(Polarization pol, Resolution res, int32_t subchannel,
		int32_t spectrum)
{
	uint32_t ofs = spectrum * getCwBytesPerSpectrum(res);
	ofs += subchannel * getCwBytesPerSubchannel(res);
	uint8_t *data = static_cast<uint8_t *> (cwBuf->getBufData(getPolCode(pol)));
	void *buf = data + ofs;
	return (buf);
}

int32_t
Channel::getTotalCwBinsPerSubchannel(Resolution res)
{
	return (state->getTotalBinsPerSubchannel(res));
}

int32_t
Channel::getCwBinsPerSubchannel(Resolution res)
{
	return (state->getUsableBinsPerSubchannel(res));
}

int32_t
Channel::getCwBytesPerSubchannel(Resolution res)
{
	return ((int32_t) getCwBinsPerSubchannel(res) * state->getCwBytesPerBin());
}

int32_t
Channel::getCwStridePerSubchannel(Resolution res)
{
	return ((int32_t) getCwBinsPerSubchannel(res));
}

int32_t
Channel::getCwBinsPerSpectrum(Resolution res)
{
	return ((int32_t) getCwBinsPerSubchannel(res) * getUsableSubchannels());
}

int32_t
Channel::getCwBytesPerSpectrum(Resolution res)
{
	return ((int32_t) getCwBinsPerSpectrum(res) * state->getCwBytesPerBin());
}

/**
 * Baseline buffer calls.
 *
 * The baseline buffers are used to normalize the average power in each
 * subchannel.  They are recomputed continually during data collection,
 * and are reported periodically to the control system.
 */
Buffer *
Channel::getBlBuf(Polarization pol)
{
	return (blBuf->getBuf(getPolCode(pol)));
}

/**
 * Return the data address of the current baseline array.
 *
 * Description:\n
 * 	Given the polarization, returns the address of the beginning
 * 	of the current baseline array.
 *
 * @param		pol polarization.
 */
float32_t *
Channel::getBlData(Polarization pol)
{
	return (static_cast<float32_t *> (blBuf->getBufData(getPolCode(pol))));
}

float32_t *
Channel::getBlData(Polarization pol, int32_t subchannel)
{
	float32_t *data = static_cast<float32_t *>
			(blBuf->getBufData(getPolCode(pol)));
	return (&data[subchannel]);
}

Buffer *
Channel::getNewBlBuf(Polarization pol)
{
	return (newBlBuf->getBuf(getPolCode(pol)));
}

/**
 * Return the data address of the new baseline array.
 *
 * Description:\n
 * 	Given the polarization, returns the address of the beginning
 * 	of the new baseline array.
 *
 * @param		pol polarization.
 */
float32_t *
Channel::getNewBlData(Polarization pol)
{
	return (static_cast<float32_t *> (newBlBuf->getBufData(getPolCode(pol))));
}

float32_t *
Channel::getNewBlData(Polarization pol, int32_t subchannel)
{
	 float32_t *data = static_cast<float32_t *>
			 (newBlBuf->getBufData(getPolCode(pol)));
	return (&data[subchannel]);
}

Buffer *
Channel::allocDetectionBuf()
{
	return (state->allocDetectionBuf());
}

Buffer *
Channel::getDetectionBuf()
{
	return (detectionBuf);
}

void
Channel::freeDetectionBuf()
{
	state->freeDetectionBuf(detectionBuf);
}

Polarization
Channel::getPol(ATADataPacketHeader::PolarizationCode polCode)
{
	return (state->getPol(polCode));
}

ATADataPacketHeader::PolarizationCode
Channel::getPolCode(Polarization pol)
{
	return (state->getPolCode(pol));
}

/**
 * Clear the baseline data buffers.
 *
 * Description:\n
 * 	Sets all the baseline data values to zero.
 */
void
Channel::clearBaselines()
{
	blBuf->initialize();
}

/**
 * Clear the input sample buffers.
 *
 * Description:\n
 * 	Removes all sample data from the sample input buffers, to allow
 * 	data collection to start with empty buffers.  Also clears the current
 * 	sequence number in preparation for new data.\n\n
 * Notes:\n
 * 	There must be no pending DFB's when this is done.
 */
void
Channel::clearInputBuffers()
{
	if (pendingList.size() > 0)
		pendingList.clear();
	if (left)
		state->freeInputBuf(left);
	left = 0;
	if (right)
		state->freeInputBuf(right);
	right = 0;
	curSeq = startSeq = 0;
}

void
Channel::clearPulseList()
{
	pulseList.clear();
}

void
Channel::addPulse(const Pulse& pulse)
{
	pulseList.push_back(pulse);
}

#ifdef notdef
/**
* Increment the half-frame counter and test for data collection complete.
*
* Description:\n
*	Add one to the half-frame counter and compare it to the total half-frame
*	count.  If it is greater than or equal to the total count, set the state
*	to DC_COMPLETE, which means the channel will ignore all incoming
*	packets.  Returns true if packet input is complete for the channel.
* Notes:\n
*	Also releases the input buffers and all packets in the queue.
*/
bool
Channel::incrementHalfFrame()
{
	return (++halfFrame >= getHalfFrames());
}
#endif

/**
* Handle a single packet for this channel
*
* Description:\n
*	Given an input packet, checks to make sure that the packet belongs
*	to this channel (fatal error if not).  If the packet is late (i.e.,
*	the current sequence number is greater than the packet sequence
*	number, the packet is discarded and the "out of sequence" counter
*	is bumped.  If the packet is not late, it is processed.\n
*	Processing consists of converting the linear (X & Y) input
*	polarization to circular (LCP & RCP), buffering the resulting
*	data until there is enough data to perform the DFB, then
*	queuing the channel to a worker thread for DFB processing.\n\n
* Notes:\n
*	Handles startup of the activity.
* 	There are two ways to handle single polarization: (1) clone the
* 	active polarization packets to make the inactive polarization
* 	packets, which results in two identical data streams, for which
* 	all processing is done except reporting signals.  (2) Create
* 	a single data stream and eliminate downstream processing of the
* 	inactive polarization.  Either method requires that the number
* 	of polarizations be checked at certain points during the
* 	processing.\n
* 	This version clones the active polarization into the inactive
* 	polarization to create a dual-pol stream.
*
* @param	pkt the input data packet.
*
* @see		addPacket
*/
Error
Channel::handlePacket_(ChannelPacket *pkt)
{
	// if we're not in an observation, discard the data without even
	// looking at it
	switch (getState()) {
	case DX_ACT_PEND_BASE_ACCUM:
	case DX_ACT_RUN_BASE_ACCUM:
	case DX_ACT_PEND_DC:
	case DX_ACT_RUN_DC:
		break;
	default:
		pktList->free(pkt);
		return (0);
	}

	// if the collection has been aborted internally, just throw away
	// all packets.  This can occur when the two polarizations are not
	// synchronized, and will eventually stop data collection.
	if (abortCollection) {
		pktList->free(pkt);
		return (0);
	}
	++stats.netStats.total;
	// validate the packet.  It must be our packet (correct src and pol)
	// and have the data valid bit set.
	ATADataPacketHeader& hdr = pkt->getHeader();
	if (hdr.src != channelSpec.src || hdr.chan != (uint32_t) channelSpec.chan) {
		++stats.netStats.wrong;
		pktList->free(pkt);
		return (0);
	}
	if (!(hdr.flags & ATADataPacketHeader::DATA_VALID)) {
		++stats.netStats.invalid;
		stats.inputStats.reset();
		pktList->free(pkt);
		curSeq = 0;
		return (0);
	}

	// reject all packets which don't match the correct polarization(s)
	if (hdr.polCode != rPol && hdr.polCode != lPol) {
		++stats.netStats.wrong;
		pktList->free(pkt);
		return (0);
	}

	// handle data collection startup
	Error err = 0;
	if (getState() == DX_ACT_PEND_BASE_ACCUM
			|| getState() == DX_ACT_PEND_DC) {
		// check for correct version number
		if (hdr.version != ATADataPacketHeader::CURRENT_VERSION) {
//			setState(DX_ACT_NONE);
			pktList->free(pkt);
			return (ERR_IPV);
		}

		int32_t t = hdr.absTime >> 32;
		NssDate start = activity->getStartTime();
#if (LOG_PACKET_TIMES)
		// send a message to the SSE logging activity start time and
		// packet time of first packtet received
		if (!armed) {
			timeval tv;
			gettimeofday(&tv, NULL);
			LogWarning(ERR_NE, activity->getActivityId(),
					"t = %u, Start time = %u, first pkt time = %u",
					tv.tv_sec, start.tv_sec, t);
		}
#endif
		if (!armed || t < start.tv_sec) {
			armed = true;
			if (t >= start.tv_sec)
				err = ERR_STAP;
			pktList->free(pkt);
			return (err);
		}
		else {
#if (LOG_PACKET_TIMES)
			// send a message to the SSE logging activity start time and
			// packet time of start packet
			timeval tv;
			gettimeofday(&tv, NULL);
			LogWarning(ERR_NE, activity->getActivityId(),
					"t = %u, Start time = %u, start pkt time = %u",
					tv.tv_sec, start.tv_sec, t);
#endif
			// start the data collection, recording the actual start time
			// and setting the packet sequence number
			if (getState() == DX_ACT_PEND_BASE_ACCUM)
				setState(DX_ACT_RUN_BASE_ACCUM);
			else
				setState(DX_ACT_RUN_DC);
			startSeq = curSeq = hdr.seq;
			// register the actual start time
			timeval s = ATADataPacketHeader::absTimeToTimeval(hdr.absTime);
			start.tv_sec = s.tv_sec;
			start.tv_usec = s.tv_usec;
			activity->setActualStartTime(start);
			// set the channel frequency from the packet header
			channelSpec.freq = hdr.freq;
			activity->setSkyFreq(hdr.freq);
		}
	}

	// test for out-of-sync between packet streams (this would normally
	// be one stream received, other not due to channelizer crash).  This
	// will stop data collection and send a message to the SSE.
	int32_t e = r.size() - l.size();
	if (!singlePol && abs(e) >= MAX_PACKET_ERROR) {
		pktList->free(pkt);
		abortCollection = true;
		return (ERR_PSU);
	}
	if (abs(e) > abs(data.err))
		data.err = e;

	// all late packets can be immediately discarded
	if (getState() != DX_ACT_RUN_BASE_ACCUM
			&& getState() != DX_ACT_RUN_DC) {
		pktList->free(pkt);
	}
	else if (hdr.seq < curSeq) {
		++stats.netStats.late;
		pktList->free(pkt);
	}
	else {
		// insert the packet
		addPacket(pkt);
		// if this is a single polarization, clone the packet for the
		// other polarization
		if (singlePol) {
			ChannelPacket *tmp = static_cast<ChannelPacket *> (pktList->alloc());
			memcpy(tmp, pkt, sizeof(ChannelPacket));
			ATADataPacketHeader& hdr = tmp->getHeader();
			hdr.polCode = inactivePol;
			addPacket(tmp);
		}
	}
	return (0);
}

/**
* Add a packet to the queue and output as many packets as possible.
*
* Description:\n
*	Called on receipt of a packet to output all possible packets.  If, at
*	the end of the process there is enough data to run the DFB,
*	the channel is scheduled for a worker task.\n\n
* Notes:\n
*	Two packet queues are maintained, one each for R and L (or X and Y).  When
*	a packet is received, it is added to the correct queue, and the
*	queues are examined.\n
*	(1) If either queue is empty, return is immediate with no processing.\n
*	(2) If the packet at the head of either queue is late, discard it.\n
*	(3) If the packets at the head of each queue have the same sequence
*	number and the number is the same as the current sequence number,
*	the packet data are converted to circular polarization and written to
*	the left and right output buffers.\n
*	(4) If the packets at the head of each queue have the same sequence
*	number but the sequence number is greater than the current sequence
*	number, matching packets have been lost, and empty packets are created,
*	converted to circular polarization and output.\n
*	(5) If the packets at the head of each queue have different sequence
*	numbers, then at least one of the polarizations has lost one or
*	more packets and empty packets must be created, converted to circular
*	polarizations.\n
*	(6) The process is repeated until one or both of the queues is
*	empty.
*
* @param	pkt input packet
*/
void
Channel::addPacket(ChannelPacket *pkt)
{
	ChannelPacket *rp, *lp;

	// add the packet to the appropriate list
	const ATADataPacketHeader& hdr = pkt->getHeader();
	switch (hdr.polCode) {
	case ATADataPacketHeader::RCIRC:
	case ATADataPacketHeader::XLINEAR:
		++data.pkts.r;
		data.lastSeq.r = hdr.seq;
		r.push(pkt);
		break;
	case ATADataPacketHeader::LCIRC:
	case ATADataPacketHeader::YLINEAR:
		++data.pkts.l;
		data.lastSeq.l = hdr.seq;
		l.push(pkt);
		break;
	default:
		Fatal(ERR_IPT);
	}

	// now process as many packet pairs as possible
	while (!r.empty() && !l.empty()) {
		// both lists have packets; discard any late packets
		rp = r.front();
		Assert(rp);
		const ATADataPacketHeader& rpHdr = rp->getHeader();
		if ((int32_t) (rpHdr.seq - curSeq) < 0) {
			++stats.netStats.late;
			--stats.netStats.missed;
			r.pop();
			pktList->free(pkt);
			continue;
		}
		lp = l.front();
		Assert(lp);
		const ATADataPacketHeader& lpHdr = lp->getHeader();
		if ((int32_t) (lpHdr.seq - curSeq) < 0) {
			++stats.netStats.late;
			--stats.netStats.missed;
			l.pop();
			pktList->free(lp);
			continue;
		}
		// we know that both lists have packets and that the heads of
		// the list are not late
		// test for r and l in sync
		if (rpHdr.seq == lpHdr.seq) {
			if (rpHdr.seq == curSeq) {
				r.pop();
				l.pop();
				addData(rp, lp);
				pktList->free(rp);
				pktList->free(lp);
			}
			else {
				// missed packets for both pols; substitute empty packets
				ChannelPacket rtmp, ltmp;
				createEmptyPacket(&rtmp, ATADataPacketHeader::RCIRC);
				createEmptyPacket(&ltmp, ATADataPacketHeader::LCIRC);
				addData(&rtmp, &ltmp);
			}
		}
		else {
			// x and y sequence numbers don't match.  We know that the x and
			// y packets are not late, so one of the pols must have lost
			// a packet.  Insert an empty packet for the missing one
			if (rpHdr.seq == curSeq) {
				// y has skipped a packet
				ChannelPacket ltmp;
				createEmptyPacket(&ltmp, ATADataPacketHeader::LCIRC);
				addData(rp, &ltmp);
				r.pop();
				pktList->free(rp);
			}
			else if (lpHdr.seq == curSeq) {
				// x has skipped a packet
				ChannelPacket rtmp;
				createEmptyPacket(&rtmp, ATADataPacketHeader::RCIRC);
				addData(&rtmp, lp);
				l.pop();
				pktList->free(lp);
			}
			else {
				// missed packets for both pols; substitute empty packets
				ChannelPacket rtmp, ltmp;
				createEmptyPacket(&rtmp, ATADataPacketHeader::RCIRC);
				createEmptyPacket(&ltmp, ATADataPacketHeader::LCIRC);
				addData(&rtmp, &ltmp);
			}
		}
	}
	// if we have enough samples, schedule a DFB
	uint64_t next = left->getNext();
	while (left->getSamples() >= threshold) {
		schedule(next);
		next += consumed;
		left->setNext(next);
		right->setNext(next);
	}
}

/**
* Convert the packet data to floating point and store in the buffers.
*
* Description:\n
* 	Accepts a synchronized pair of packets (right + left or x + y),
* 	converts them to single-precision floating point complex and stores
* 	them in the input buffers.  When there is not enough space in the
* 	buffers to store the data, completed samples are flushed to make
* 	room.\n
* Notes:\n
* 	No polarization conversion is performed, so linear and circular
* 	polarization are maintained.
*/
void
Channel::addData(ChannelPacket *rp, ChannelPacket *lp)
{
#if CHANNEL_TIMING
	uint64_t t0 = getticks();
#endif
	Assert(rp);
	Assert(lp);
	Assert(right);
	Assert(left);
	const ATADataPacketHeader& rpHdr = rp->getHeader();
	const ATADataPacketHeader& lpHdr = lp->getHeader();
	ASSERT(rpHdr.len == lpHdr.len);
//	complexInt16 *xData = reinterpret_cast<complexInt16 *> (rp->getData());
//	complexInt16 *yData = reinterpret_cast<complexInt16 *> (lp->getData());
//	complexFloat32 j(0, 1);

//	left->flush();
//	right->flush();

	// flush only when necessary
	if (right->getFree() < rpHdr.len || left->getFree() < lpHdr.len) {
		lock();
		// flush as many iterations as possible
		uint64_t rDone = right->getDone();
		uint64_t lDone = left->getDone();
		uint64_t rNext = right->getNext();
		uint64_t lNext = left->getNext();
		while (!pendingList.empty()) {
			PendingList::iterator p = pendingList.begin();
			// flush as many iterations as possible; an iteration can be
			// flushed when the DFB using its data has been completed.
			if (p->second) {
				Assert(lDone <= p->first);
				Assert(rDone <= p->first);
				if (p->first > lNext) {
//					uint64_t pFirst = p->first;
					Assert(p->first <= lNext);
				}
				Assert(p->first <= rNext);
				left->setDone(lDone = p->first);
				right->setDone(rDone = p->first);
				pendingList.erase(p);
			}
			else
				break;
		}
		unlock();
		++flushes;
		// there'd better be room now
		if (right->getFree() < rpHdr.len || left->getFree() < lpHdr.len)
			Fatal(ERR_IBO);
	}
#if CHANNEL_TIMING
	uint64_t t1 = getticks();
#endif

	// copy the sample data into the input buffers.  The data is simply
	// copiedThe data is converted
	// to single-precision floating point prior to storage.  Since the
	// buffer length
	// is a multiple of the packet sample length, there should always
	// be enough room to do the copy without wrapping.
	ComplexInt16 *rData = static_cast<ComplexInt16 *>
			(right->getWrite(lpHdr.len));
	Assert(rData);
	ComplexInt16 *lData = static_cast<ComplexInt16 *>
			(left->getWrite(lpHdr.len));
	Assert(lData);
	memcpy(rData, rp->getSamples(), rp->getDataSize());
	memcpy(lData, lp->getSamples(), lp->getDataSize());
#if CHANNEL_TIMING
	uint64_t t2 = getticks();
#endif
#ifdef notdef
	for (int32_t i = 0; i < rpHdr.len; ++i) {
		rData[i] = rp->getSample(i);
		lData[i] = lp->getSample(i);
	}
#endif
	sampleCnt += rpHdr.len;
	right->setLast(lpHdr.len);
	left->setLast(lpHdr.len);
	++curSeq;
#if CHANNEL_TIMING
	uint64_t t3 = getticks();
	++timing.packets;
	float t = elapsed(t1, t0);
	if (t > timing.maxFlush)
		timing.maxFlush = t;
	timing.flush += t;
	timing.store += elapsed(t2, t1);
	timing.set += elapsed(t3, t2);
	timing.total += elapsed(t3, t0);
#endif
}

/**
 * Schedule a DFB iteration.
 *
 * Description:\n
 * 	Sends a message to the work queue specifying that a DFB iteration
 * 	should be performed beginning at the data sample specified.\n\n
 * Notes:\n
 *
 */
void
Channel::schedule(uint64_t sample)
{
	// add the starting sample to the pending list
	lock();
	pendingList.insert(std::make_pair(sample, false));
	unlock();
	MemBlk *blk = partitionSet->alloc(sizeof(HalfFrameInfo));
	Assert(blk);
	HalfFrameInfo *hfInfo = static_cast<HalfFrameInfo *> (blk->getData());
	hfInfo->channel = this;
	hfInfo->halfFrame = inputHalfFrame++;
	hfInfo->sample = sample;
	Msg *msg = msgList->alloc((DxMessageCode) DfbProcess,
			getActivityId(), hfInfo, sizeof(HalfFrameInfo), blk);
	Assert(!workQ->send(msg, 0));
	++schedules;
}

void
Channel::createEmptyPacket(ChannelPacket *pkt, uint8_t pol)
{
	++stats.netStats.missed;
	ATADataPacketHeader& hdr = pkt->getHeader();
	hdr.seq = curSeq;
	hdr.polCode = pol;
//	pkt->putHeader(hdr);
	memset(pkt->getSamples(), 0, pkt->getDataSize());
}

/**
* Perform DFB filtering and subchannelization.
*
* Description:\n
*	Filters the input data using a digital filter bank (DFB), converting to
*	floating point and optionally swapping real and imaginary to invert
*	the spectrum.  Then performs an FFT to produce subchannels.\n
* Notes:\n
* 	Performs the DFB on both polarizations if the observation is dual-pol,
* 	otherwise does only the active polarization.
*/
void
Channel::dfbProcess(uint64_t sample, ComplexFloat32 *sampleBuf,
		ComplexFloat32 **rBuf, ComplexFloat32 **lBuf)
{
#if CHANNEL_TIMING
	uint64_t t0 = getticks();
#endif

	int32_t n;
#ifdef notdef
	if (singlePol) {
		if (inactivePol == ATADataPacketHeader::LCIRC ||
				inactivePol == ATADataPacketHeader::YLINEAR) {
			n = dfbPol(sample, right, sampleBuf, rBuf);
		}
		else
			n = dfbPol(sample, left, sampleBuf, lBuf);
	}
	else {
#endif
		// both polarizations
		n = dfbPol(sample, right, sampleBuf, rBuf);
		n = dfbPol(sample, left, sampleBuf, lBuf);
#ifdef notdef
	}
#endif
#if CHANNEL_TIMING
	uint64_t t1 = getticks();
#endif

	// mark the DFB entry in the pending list as complete
	lock();
	PendingList::iterator p = pendingList.find(sample);
	Assert(p != pendingList.end());
	p->second = true;
	++dones;
	unlock();
#if CHANNEL_TIMING
	uint64_t t2 = getticks();
	++timing.dfb.dfbs;
	timing.dfb.dfb += elapsed(t1, t0);
	timing.dfb.list += elapsed(t2, t1);
	timing.dfb.total += elapsed(t2, t0);
#endif
}

/**
 * Flush the DFB buffer without doing a DFB.
 *
 * Description:\n
 * 	Logs the DFB as complete by simply setting its pending list entry to done.
 */
void
Channel::dfbFlush(uint64_t sample)
{
	lock();
	PendingList::iterator p = pendingList.find(sample);
	Assert(p != pendingList.end());
	p->second = true;
	++dones;
	unlock();
}

/**
 * Perform a DFB on a single polarization.
 *
 * Description:\n
 * 	Converts the data to single precision floating point, optionally swaps
 * 	real and imaginary, and stores the data in a working buffer.  Then
 * 	performs a DFB::iterate on the buffer to produce the output.  Returns
 * 	the total number of samples consumed; this should be the same as threshold.\n\n
 * Notes:\n
 * 	Since the input data is in a circular buffer, it may be necessary to
 * 	wrap past the end of the buffer and do the copy in two parts.
 */
int32_t
Channel::dfbPol(uint64_t sample, InputBuffer *inBuf,
		ComplexFloat32 *sampleBuf, ComplexFloat32 **buf)
{
#if CHANNEL_TIMING
	uint64_t t0 = getticks();
#endif

	// copy the input data to the DFB buffer.  Since the input buffer
	// is circular, it may be necessary to wrap around to the beginning
	// during the copy, so we have two loops.
	int32_t len = threshold;
	ComplexInt16 *samples = static_cast<ComplexInt16 *>
			(inBuf->getSampleBlk(sample, len));
	if (swap)
			getSwappedSamples(sampleBuf, samples, len);
	else
			getSamples(sampleBuf, samples, len);
	uint64_t s = sample + len;
	int32_t len2 = threshold - len;
	if (len2) {
		samples = static_cast<ComplexInt16 *> (inBuf->getSampleBlk(s, len2));
		if (swap)
			getSwappedSamples(sampleBuf + len, samples, len2);
		else
			getSamples(sampleBuf + len, samples, len2);
	}
#if CHANNEL_TIMING
	uint64_t t1 = getticks();
	timing.dfb.list += elapsed(t1, t0);
#endif

	// perform the DFB
	// DFB is performed on linear sample buffer
	ComplexFloat32 *td[1];
	td[0] = sampleBuf;
	int32_t n = dfb.iterate((const ComplexFloat32 **) td, 1, threshold, buf);
	return (n);
}

void
Channel::getSamples(ComplexFloat32 *sampleBuf, ComplexInt16 *samples,
		int32_t len)
{
	for (int32_t i = 0; i < len; ++i)
		sampleBuf[i] = samples[i];
}

void
Channel::getSwappedSamples(ComplexFloat32 *sampleBuf, ComplexInt16 *samples,
		int32_t len)
{
	for (int32_t i = 0; i < len; ++i)
		sampleBuf[i] = ComplexFloat32(samples[i].imag(), samples[i].real());
}

}
