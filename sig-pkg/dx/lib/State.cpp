/*******************************************************************************

 File:    State.cpp
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
// DX state class
//
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/State.cpp,v 1.10 2009/06/16 15:14:18 kes Exp $
//
#include <unistd.h>
#include "Activity.h"
#include "Args.h"
#include "Dfb.h"
#include "DxErr.h"
#include "DxTypes.h"
#include "DxUtil.h"
#include "State.h"
#include "System.h"

using namespace sonata_lib;
using namespace dfb;
using sonata_lib::Log;

namespace dx {

State *State::instance = 0;

State *
State::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new State();
	l.unlock();
	return (instance);
}

State::State(): number(-1), maxFrames(0),
		binsPerSubchannel1Hz(TOTAL_BINS_PER_SUBCHANNEL_1HZ), err(0),
		sLock("stateLock"), activityList(0), hfBufPairList(0), detectionBuf(0),
		archiveBufPairList(0), candBufPairList(0), inputBufList(0),
		permRfiMask(0), birdieMask(0), rcvrBirdieMask(0), testSignalMask(0),
		recentRfiMask(0)
{
	init();
}

State::~State()
{
	delete hfBufPairList;
	delete detectionBuf;
	delete activityList;
}

/**
 * Initialize the system state.
 *
 * Description:\n
 * 	Initializes the system state, initializing internal variables based
 * 	on startup parameters
 */
void
State::init()
{
	Args *args = Args::getInstance();
	Assert(args);

	/////////////////////////////////////////////////////////////////////
	// IMPORTANT: perform these three initializations first, and in this
	// order, to insure that proper values are set.
	/////////////////////////////////////////////////////////////////////
	// get the channel information
	channelSpec.init(args->getChannelBandwidth(),
			args->getChannelOversampling());

	// get the subchannel information
	subchannelSpec.init(getChannelWidthMHz(), getChannelOversampling(),
			args->getSubchannels(), args->getOversampling(),
			args->getFoldings());


	// get the 1Hz bin information
	binSpec.init(getSubchannelWidthMHz(), getSubchannelOversampling(),
			getTotalBinsPerSubchannel(RES_1HZ), DEFAULT_BIN_OVERSAMPLING);

	maxFrames = args->getMaxFrames();

#ifndef notdef
	// get the DFB parameters so that we can compute input buffer size
	Dfb dfb;
	if (args->useCustomFilter()) {
		const FilterSpec& filter = args->getFilter();
		dfb.setCoeff(filter.coeff, filter.fftLen, filter.foldings);
	}
	dfb.setup(getTotalSubchannels(),
			getTotalSubchannels() * getSubchannelOversampling(),
			getFoldings(), getSamplesPerSubchannelHalfFrame());
	DfbInfo dfbInfo;
	dfb.getInfo(&dfbInfo);

	// allocate the input buffers; note that the buffer length must be
	// a multiple of the number of samples in a channel packet
	size_t threshold = dfb::Dfb::getThreshold(getTotalSubchannels(),
			getFoldings(), getTotalSubchannels() * getSubchannelOversampling(),
			getSamplesPerSubchannelHalfFrame());
	int32_t packets = threshold / ATADataPacketHeader::CHANNEL_SAMPLES + 1;
	size_t size = BUF_COUNT * packets * ATADataPacketHeader::CHANNEL_SAMPLES;
	inputBufList = new InputBufferList("inputBuf", INPUT_BUFFERS, size,
			sizeof(ComplexInt16));
	Assert(inputBufList);
#else
	size_t size;
#endif
	// create the half frame buffer pair list
	size = getTotalSubchannels() * getBytesPerSubchannelHalfFrame();
	hfBufPairList = new BufPairList("hfBufPair", HALF_FRAME_BUFFERS, size);
	Assert(hfBufPairList);

	// create the detection buffer
	size = computeDetectionBufSize(maxFrames,
			MAX_SPECTRA_PER_FRAME_4HZ, getUsableBinsPerSpectrum(RES_4HZ));
	detectionBuf = new Buffer("detectionBuf", size, false);
	Assert(detectionBuf);

	// allocate a list of archive channels, which are ComplexFloat32 and
	// must be large enough to hold a maximum-length activity
	size = getSubchannelsPerArchiveChannel() * getMaxHalfFrames()
			* getSamplesPerSubchannelHalfFrame() * sizeof(ComplexFloat32);
	archiveBufPairList = new BufPairList("archive channel", ARCHIVE_BUFS,
			size);
	Assert(archiveBufPairList);

	// allocate a list of candidate buffers, which are the data actually
	//  written to the archiver.  They are ComplexPair.
	size = getSubchannelsPerArchiveChannel() * getMaxHalfFrames()
			* getSamplesPerSubchannelHalfFrame() * sizeof(ComplexPair);
	candBufPairList = new BufPairList("candidate archive channel",
			CANDIDATE_BUFS, size);
	Assert(candBufPairList);

	// set the intrinsics
	strcpy(intrinsics.interfaceVersionNumber, SSE_DX_INTERFACE_VERSION);
	intrinsics.maxSubchannels = getUsableSubchannels();
	intrinsics.hzPerSubchannel = MHZ_TO_HZ(getSubchannelWidthMHz());
	// test for specific host name on command line
	gethostname(intrinsics.host, sizeof(intrinsics.host));
	if (args->getName().length())
		strcpy(intrinsics.name, args->getName().c_str());
	else
		strcpy(intrinsics.name, intrinsics.host);
	strcpy(intrinsics.channelBase.addr, args->getMcAddr().c_str());
	intrinsics.channelBase.port = args->getMcPort();
	intrinsics.foldings = getFoldings();
	intrinsics.oversampling = getSubchannelOversampling();
	char tmp[MAX_TEXT_STRING];
	strcpy(tmp, args->getFilterFile().c_str());
	strcpy(intrinsics.filterName, basename(tmp));
	char *p;
	for (p = intrinsics.name; *p && !isdigit(*p); ++p)
		;
	if (*p)
		intrinsics.serialNumber = atoi(p);
	else
		intrinsics.serialNumber = 0;
	number = intrinsics.serialNumber;
	strcpy(intrinsics.codeVersion, CODE_VERSION);
	if (birdieMask)
		intrinsics.birdieMaskDate = birdieMask->getDate();
	if (rcvrBirdieMask)
		intrinsics.rcvrBirdieMaskDate = rcvrBirdieMask->getDate();
	if (permRfiMask)
		intrinsics.permMaskDate = permRfiMask->getDate();

	// create the activity list
	activityList = ActivityList::getInstance(this, MAX_ACTIVITIES);
	Assert(activityList);
}

/**
 * Configure the DX.
 *
 * Description:\n
 * 	Configure the DX for operation.  This involves using the configuration
 *	information from the SSE to compute the number of subchannels and the
 * 	width of each subchannel.  Information provided on the command line
 * 	allows computation of the bin widths and frame times.\n\n
 */
void
State::configure(DxConfiguration *configuration_)
{
	lock();
	configuration = *configuration_;
//	computeSubchannelData();
//	computeBinData();
//	computeFrameData();
	unlock();
}

const DxConfiguration&
State::getConfiguration()
{
	return (configuration);
}

/**
 * Get the system intrinsics.
 *
 * Description:\n
 * 	Returns the system intrinsics.
 */
const DxIntrinsics&
State::getIntrinsics()
{
	return (intrinsics);
}

DxStatus
State::getStatus()
{
	lock();
	DxStatus status_;
	GetNssDate(status_.timestamp);
	Activity *act;
	int32_t i;
	for (i = 0, act = activityList->getFirst(); act; ++i,
			act = activityList->getNext()) {
		status_.act[i].activityId = act->getActivityId();
		status_.act[i].currentState = act->getState();
	}
	status_.numberOfActivities = i;
	unlock();
	return (status_);
}

const char8_t *
State::getArchiverHost()
{
	return (configuration.archiverHostname);
}

int32_t
State::getArchiverPort()
{
	return (configuration.archiverPort);
}

/**
 * Compute the total bins per subchannel for a given resolution.
 */
int32_t
State::getTotalBinsPerSubchannel(Resolution res)
{
	if (res < RES_1HZ || res > RES_1KHZ)
		Fatal(ERR_IR);
	int32_t bins = getTotalBinsPerSubchannel1Hz() >> (res - RES_1HZ);
	return (bins);
}
/**
 * Get the number of bins per subchannel for a specific resolution.
 *
 */
int32_t
State::getUsableBinsPerSubchannel(Resolution res)
{
	if (res < RES_1HZ || res > RES_1KHZ)
		Fatal(ERR_IR);
	int32_t bins = lrintf((float32_t) binSpec.usable / (1 << (res - RES_1HZ)));
	return (bins);
}

/**
 * Get the number of bins per spectrum for a specific resolution.
 */
int32_t
State::getUsableBinsPerSpectrum(Resolution res)
{
	lock();
	int32_t bins = getUsableBinsPerSubchannel(res) * getUsableSubchannels();
	unlock();
	return (bins);
}

/**
 * Get the bin width of the specified resolution.
 *
 * Notes:\n
 * 	This computation is NOT CORRECT for bins above a certain resolution;
 * 	the resolution depends upon the oversampling used when the subchannels
 * 	are created.  For 25% oversampling and 1024 1Hz bins per subchannel,
 * 	the 512Hz resolution will be incorrect, but all coarser resolutions
 * 	will be correct.
 */
float64_t
State::getBinWidthMHz(Resolution res)
{
	return (HZ_TO_MHZ(getBinWidthHz(res)));
}

float64_t
State::getBinWidthHz(Resolution res)
{
	return (binSpec.widthHz * (1 << res));
}

//
// allocActivity: allocate an activity
//
// Notes:
//		The activity is initialized to the DX_ACT_NONE state and is
//		placed in the allocated list.
//
Activity *
State::allocActivity(const DxActivityParameters *params_)
{
	Activity *act = activityList->alloc();;
	if (act)
		act->setup(*params_);
	else
		LogError((DxMessageCode) ERR_NFA, -1, "");

	return (act);
}

//
// freeActivity: release the specified activity
//
// Notes:
//		The activity must be removed from the allocated list, then
//		added to the free list.
//		If the activity is not idle, then a warning is issued.
//
void
State::freeActivity(Activity *act)
{
	int32_t activityId = act->getActivityId();

	lock();

	// if the activity is not idle, issue a warning before deleting it
	if (act->getState() != DX_ACT_NONE) {
		LogError((DxMessageCode) ERR_ANI, activityId,
				"activity %d", activityId);
	}

	// release all resources owned by the activity
	act->free();

	// now free the activity
	Error err = activityList->free(act);
	unlock();
	if (err) {
		LogError((DxMessageCode) ERR_NSA, activityId, "activity %d",
				activityId);
	}
}

Activity *
State::getFirstActivity()
{
	lock();
	Activity *act = activityList->getFirst();
	unlock();
	return (act);
}

Activity *
State::getNextActivity()
{
	lock();
	Activity *act = activityList->getNext();
	unlock();
	return (act);
}

Activity *
State::findActivity(int32_t activityId)
{
	Debug(DEBUG_STATE, activityId, "find act");
	lock();
	Debug(DEBUG_STATE, activityId, "find act, locked");
	Activity *act = activityList->find(activityId);
	Debug(DEBUG_STATE, activityId, "find act, found");
	unlock();
	Debug(DEBUG_STATE, (void *) act, "act");
	return (act);
}

Activity *
State::findActivity(DxActivityState state)
{
	lock();
	Activity *act;
	for (act = activityList->getFirst(); act; act = activityList->getNext()) {
		if (act->getState() == state)
			break;
	}
	unlock();
	return (act);
}

/**
 * Mask functions.
 */
void
State::setPermRfiMask(const FrequencyMaskHeader *hdr_)
{
	lock();
	// decrement the use count on the old mask, if there is one
	if (permRfiMask && !permRfiMask->decrementUseCount())
		delete permRfiMask;
	const FrequencyBand *mask = reinterpret_cast<const FrequencyBand *>
			(hdr_ + 1);
	Debug(DEBUG_STATE, hdr_->maskVersionDate.tv_sec, "mask date");
	Debug(DEBUG_STATE, hdr_->bandCovered.centerFreq, "mask ctr freq");
	Debug(DEBUG_STATE, hdr_->bandCovered.bandwidth, "mask bandwidth");
	Debug(DEBUG_STATE, mask->centerFreq, "first ctr freq");
	Debug(DEBUG_STATE, mask->bandwidth, "first bandwidth");
	permRfiMask = new PermRfiMask(*hdr_, mask);
	intrinsics.permMaskDate = permRfiMask->getDate();
	// if there's a defined activity, change its mask
	if (Activity *act = findActivity(DX_ACT_INIT))
		act->setPermRfiMask(permRfiMask);
	unlock();
}

PermRfiMask *
State::getPermRfiMask()
{
	lock();
	PermRfiMask *permRfiMask_ = permRfiMask;
	if (permRfiMask)
		permRfiMask->incrementUseCount();
	unlock();
	return (permRfiMask_);
}

void
State::setBirdieMask(const FrequencyMaskHeader *hdr_)
{
	lock();
	// decrement the use count on the old mask, if there is one
	if (birdieMask && !birdieMask->decrementUseCount())
		delete birdieMask;
	const FrequencyBand *mask = reinterpret_cast<const FrequencyBand *>
			(hdr_ + 1);
	birdieMask = new BirdieMask(*hdr_, mask);
	intrinsics.birdieMaskDate = birdieMask->getDate();
	// if there's a defined activity, change its mask
	if (Activity *act = findActivity(DX_ACT_INIT))
		act->setBirdieMask(birdieMask);
	unlock();
}

BirdieMask *
State::getBirdieMask()
{
	lock();
	BirdieMask *birdieMask_ = birdieMask;
	if (birdieMask)
		birdieMask->incrementUseCount();
	unlock();
	return (birdieMask_);
}

void
State::setRcvrBirdieMask(const FrequencyMaskHeader *hdr_)
{
	lock();
	// decrement the use count on the old mask, if there is one
	if (rcvrBirdieMask && !rcvrBirdieMask->decrementUseCount())
		delete rcvrBirdieMask;
	const FrequencyBand *mask = reinterpret_cast<const FrequencyBand *>
			(hdr_ + 1);
	Debug(DEBUG_STATE, hdr_->maskVersionDate.tv_sec, "mask date");
	Debug(DEBUG_STATE, hdr_->bandCovered.centerFreq, "mask ctr freq");
	Debug(DEBUG_STATE, hdr_->bandCovered.bandwidth, "mask bandwidth");
	Debug(DEBUG_STATE, hdr_->numberOfFreqBands, "mask band count");
	Debug(DEBUG_STATE, mask->centerFreq, "first ctr freq");
	Debug(DEBUG_STATE, mask->bandwidth, "first bandwidth");
	rcvrBirdieMask = new BirdieMask(*hdr_, mask);
	intrinsics.rcvrBirdieMaskDate = rcvrBirdieMask->getDate();
	// if there's a defined activity, change its mask
	if (Activity *act = findActivity(DX_ACT_INIT))
		act->setRcvrBirdieMask(birdieMask);
	unlock();
}

BirdieMask *
State::getRcvrBirdieMask()
{
	lock();
	BirdieMask *rcvrBirdieMask_ = rcvrBirdieMask;
	if (rcvrBirdieMask)
		rcvrBirdieMask->incrementUseCount();
	unlock();
	return (rcvrBirdieMask_);
}

void
State::setRecentRfiMask(const RecentRfiMaskHeader *hdr_)
{
	lock();
	// decrement the use count on the old mask, if there is one
	if (recentRfiMask && !recentRfiMask->decrementUseCount())
		delete recentRfiMask;
	const FrequencyBand *mask = reinterpret_cast<const FrequencyBand *>
			(hdr_ + 1);
	recentRfiMask= new RecentRfiMask(*hdr_, mask);
	// if there's a defined activity, change its mask
	if (Activity *act = findActivity(DX_ACT_INIT))
		act->setRecentRfiMask(recentRfiMask);
	unlock();
}

RecentRfiMask *
State::getRecentRfiMask()
{
	lock();
	RecentRfiMask *recentRfiMask_ = recentRfiMask;
	if (recentRfiMask)
		recentRfiMask->incrementUseCount();
	unlock();
	return (recentRfiMask_);
}

void
State::setTestSignalMask(const FrequencyMaskHeader *hdr_)
{
	lock();
	// decrement the use count on the old mask, if there is one
	if (testSignalMask && !testSignalMask->decrementUseCount())
		delete testSignalMask;
	const FrequencyBand *mask = reinterpret_cast<const FrequencyBand *>
			(hdr_ + 1);
	testSignalMask = new TestSignalMask(*hdr_, mask);
	// if there's a defined activity, change its mask
	if (Activity *act = findActivity(DX_ACT_INIT))
		act->setTestSignalMask(testSignalMask);
	unlock();
}

TestSignalMask *
State::getTestSignalMask()
{
	lock();
	TestSignalMask *testSignalMask_ = testSignalMask;
	if (testSignalMask)
		testSignalMask->incrementUseCount();
	unlock();
	return (testSignalMask_);
}

/**
 * Input buffer functions.
 *
 * Description:\n
 * 	Manages allocation and deallocation of the input buffers.\n
 * Notes:\n
 * 	Since only one data collection can be active at a time, we only
 * 	need one pair of input buffers, which are allocated by the channel
 * 	at the start of data collection and freed when data collection is
 * 	complete.
 */
InputBuffer *
State::allocInputBuf(bool wait_)
{
	InputBuffer *buf = inputBufList->alloc(wait_);
	return (buf);
}

void
State::freeInputBuf(InputBuffer *buf)
{
	inputBufList->free(buf);
}

/**
 * Half frame buffer functions.
 *
 * Description:\n
 * 	Manages allocation and deallocation of pairs of half frame buffers.\n
 * Notes:\n
 * 	Half frame buffers come in pairs (right and left), and are used
 * 	only during data collection.  Since only one activity can be in
 * 	data collection, only a single set of buffer pairs is required.\n
 * 	Enough buffer pairs must be allocated for a call to the spectrometer
 * 	(typically 3 half frames), as well as one buffer pair for current
 * 	data input.
 */
BufPair *
State::allocHfBuf(bool wait_)
{
//	ssize_t size = hfBufPairList->getSize();
	return (hfBufPairList->alloc(wait_));
}

void
State::freeHfBuf(BufPair *bufPair)
{
	bufPair->release();
//	ssize_t size = hfBufPairList->getSize();
}

/**
 * Allocate a detection buffer.
 *
 * Description:\n
 * 	Allocates a detection buffer for signal detection.\n
 * Notes:\n
 * 	The detection buffer is used only during signal detection and
 * 	confirmation.  Since only a single activity can be in any of
 * 	these states at one time, only a single detection buffer is
 * 	needed.\n
 * 	The detection buffer must be large enough to handle CW detection
 * 	for the longest possible activity.
 */
Buffer *
State::allocDetectionBuf()
{
	if (detectionBuf->isFull())
		return (0);
	detectionBuf->setFull();
	return (detectionBuf);
}

void
State::freeDetectionBuf(Buffer *buf_)
{
	Assert(detectionBuf == buf_);
	detectionBuf->setEmpty();
}

/**
 * Allocate an archive buffer.
 */
BufPair *
State::allocArchiveBuf(bool wait_)
{
	return (archiveBufPairList->alloc(wait_));
}

void
State::freeArchiveBuf(BufPair *bufPair)
{
	bufPair->release();
}

/**
 * Allocate a candidate buffer.
 *
 * Notes:\n
 * 	A candidate buffer contains the same data as the archive buffer, but
 * 	has been converted to ComplexPair (4-bit integer complex) format.
 */
BufPair *
State::allocCandBuf(bool wait_)
{
	return (candBufPairList->alloc(wait_));
}

void
State::freeCandBuf(BufPair *bufPair)
{
	bufPair->release();
}

/**
 * Functions to convert between ATADataPacketHeader and SSE versions
 * 	of polarization.
 */
/**
 * Convert a PolarizationCode to a Polarization.
 *
 */
Polarization
State::getPol(ATADataPacketHeader::PolarizationCode polCode)
{
	Polarization pol = POL_UNINIT;

	switch (polCode) {
	case ATADataPacketHeader::RCIRC:
	case ATADataPacketHeader::XLINEAR:
		pol = POL_RIGHTCIRCULAR;
		break;
	case ATADataPacketHeader::LCIRC:
	case ATADataPacketHeader::YLINEAR:
		pol = POL_LEFTCIRCULAR;
		break;
	default:
		break;
	}
	return (pol);
}

/**
 * Convert a Polarization to a PolarizationCode.
 */

ATADataPacketHeader::PolarizationCode
State::getPolCode(Polarization pol)
{
	ATADataPacketHeader::PolarizationCode polCode =
			(ATADataPacketHeader::PolarizationCode)
			ATADataPacketHeader::UNDEFINED;
	switch (pol) {
	case POL_RIGHTCIRCULAR:
		polCode = ATADataPacketHeader::RCIRC;
		break;
	case POL_LEFTCIRCULAR:
		polCode = ATADataPacketHeader::LCIRC;
		break;
	default:
		break;
	}
	return (polCode);
}

/**
 * Initialize the subchannel specification.
 *
 * Description:\n
 * 	Given the effective (not nominal) width of a channel, the oversampling
 * 	of the channel data, the total number of subchannels and the oversampling
 * 	of the subchannels, compute the nominal and effective widths of the
 * 	subchannels.\n\nm
 *
 * Notes:\n
 * 	The effective width of a channel is greater than the nominal width due
 * 	to oversampling of the channel, so some of the bandwidth is unusable;
 * 	the amount is the same as the oversampling percentage.  Thus, when
 * 	subchannels are created from the channel data, some of the edge
 * 	subchannels are unusable and must be discarded.
 *
 * @param		cw channel width.
 * @param		co channel oversampling percentage.
 * @param		t total # of subchannels.
 * @param		o subchannel oversampling percentage.
 * @param		f # of foldings
 */
void
State::sSpec::init(float64_t cw, float64_t co, int32_t t, float64_t o,
		int32_t f)
{
	total = t;
	foldings = f;
	oversampling = o;
	usable = (int32_t) (total * (1.0 - co));
	widthMHz = cw / usable;
	effectiveWidthMHz = widthMHz / (1.0 - oversampling);
}

/**
 * Initialze the bin specification.
 *
 * Description:\n
 * 	Given the width of a subchannel, the oversampling of the subchannel,
 * 	the total number of bins to create and the oversampling of the
 * 	spectrometer, compute the bin width and the number of usable channels.\n\n
 *
 *
 * @param		sw subchannel width.
 * @param		so subchannel oversampling.
 * @param		t total # of bins.
 * @param		o oversampling of the bins.
 */
void
State::bSpec::init(float64_t sw, float64_t so, int32_t t, float64_t o)
{
	total = t;
	oversampling = o;
	usable = total * (1.0 - so);
	widthHz = MHZ_TO_HZ(sw) / usable;
	effectiveWidthHz = widthHz / (1.0 - oversampling);
}

}
