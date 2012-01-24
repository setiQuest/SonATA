/*******************************************************************************

 File:    Activity.cpp
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
// Activity class definition
//
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/Activity.cpp,v 1.8 2009/05/24 23:38:46 kes Exp $
//

#include "Activity.h"
#include "Channel.h"
#include "CwSignal.h"
#include "DxErr.h"
#include "CwFollowupSignal.h"
#include "PulseFollowupSignal.h"
#include "Log.h"
#include "DxOpsBitset.h"
#include "PulseSignal.h"
#include "State.h"

using namespace sonata_lib;

namespace dx {

Activity::Activity(State *state_): resolutions(0), candidatesOverMax(0),
		followupSignals(0), frames(0), halfFrames(0), channel(state_, this),
		type(NormalActivity), aLock("activityLock"),
		mode(PRIMARY), activityState(DX_ACT_NONE),
		birdieMask(0), rcvrBirdieMask(0), recentRfiMask(0), permRfiMask(0),
		testSignalMask(0), candidateList(0), superClusterer(0),
		args(0), state(state_)

{
	args = Args::getInstance();
	Assert(args);
	setActivityId(-1);
	cwBadBandList.clear();
	for (int i = 0; i < MAX_RESOLUTIONS; ++i)
		pulseBadBandList[i].clear();
}

Activity::~Activity()
{
	releaseCandidates();
	releaseSignals();
	releaseFollowupSignals();
	releaseBadBands();
}

/**
 * Set up the activity for a new observation.
 *
 * Description:\n
 * 	Pass in the DxActivityParameters for the new observation and set up
 * 	the activity.
 */
void
Activity::setup(const DxActivityParameters& params_)
{
	lock();
	params = params_;

	// allocate a new superclusterer
	superClusterer = new SuperClusterer;
	Assert(superClusterer);

	// now build the list of resolutions
	resolutions = 0;
	for (int32_t i = 0; i < MAX_RESOLUTIONS; ++i) {
		resData[i].res = RES_UNINIT;
		if (params.requestPulseResolution[i] || i == params.daddResolution) {
			// we're doing this resolution; build the initialization entry
			Resolution res = (Resolution) i;
			resData[resolutions].res = res;
			resData[resolutions].fftLen = state->getTotalBinsPerSubchannel(res);
			if (res != RES_1KHZ)
				resData[resolutions].overlap = true;
			else
				resData[resolutions].overlap = false;
			++resolutions;
		}
	}
	setState(DX_ACT_INIT);
	setPermRfiMask(state->getPermRfiMask());
	setBirdieMask(state->getBirdieMask());
	setRcvrBirdieMask(state->getRcvrBirdieMask());
	setRecentRfiMask(state->getRecentRfiMask());
	setTestSignalMask(state->getTestSignalMask());
	initRequestedSubchannelMask();
	initDoneMask();
	resetDetectionStatistics();
	setCandidatesOverMax(0);
	setFollowupSignals(0);
	selectCandidateList(PRIMARY);
	setMode(PRIMARY);
	// compute and set the number of frames in the activity
	int32_t f = (int32_t) (params.dataCollectionLength / getFrameTime());
#ifdef notdef
	if ((f + 0.5) * getFrameTime() > params.dataCollectionLength)
		--f;
#endif
	frames = (f < 0) ? 0 : f;
	halfFrames = 2 * frames + 1;
	NssDate start;
	start.tv_sec = start.tv_usec = -1;
	setStartTime(start);
	channel.setup(params, args->getPol());
	unlock();
}

/**
 * Release all resources owned by the activity
*/
void
Activity::free()
{
	// delete the superclusterer
	delete superClusterer;
	superClusterer = 0;

	channel.clearInputBuffers();
	releaseCandidates();
	releaseSignals();
	releaseFollowupSignals();
	releaseBadBands();

	// free any masks in use by the activity
	setPermRfiMask(0);
	setBirdieMask(0);
	setRcvrBirdieMask(0);
	setRecentRfiMask(0);
	setTestSignalMask(0);
}

float64_t
Activity::getFrameTime()
{
	return (state->getFrameTime());
}

float64_t
Activity::getFramesPerSec()
{
	return (1.0 / getFrameTime());
}

float64_t
Activity::getChannelWidthMHz()
{
	return (state->getChannelWidthMHz());
}

float64_t
Activity::getChannelWidthHz()
{
	return (MHZ_TO_HZ(getChannelWidthMHz()));
}

int32_t
Activity::getUsableSubchannels()
{
	return (state->getUsableSubchannels());
}

float64_t
Activity::getSubchannelWidthMHz()
{
	return (state->getSubchannelWidthMHz());
}

/**
 * Get the number of bins per subchannel at a specified resolution.
 *
 * Description:\n
 * 	Computes the number of bins per subchannel at a specified resolution.\n\n
 *
 * Notes:\n
 * 	Calls State::getBinsPerSubchannel.
 *
 * @param		res resolution.
 */
int32_t
Activity::getUsableBinsPerSubchannel(Resolution res)
{
	return (state->getUsableBinsPerSubchannel(res));
}

/**
 * Get the number of bins per spectrum at a specified resolution.
 *
 * Description:\n
 * 	Computes the total number of bins in a full-bandwidth spectrum at
 * 	the specified resolution.\n\n
 *
 * Notes:\n
 * 	Calls State::getBinsPerSpectrum.
 *
 * @param		res resolution.
 */
int32_t
Activity::getUsableBinsPerSpectrum(Resolution res)
{
	return (state->getUsableBinsPerSpectrum(res));
}

float64_t
Activity::getBinWidthMHz(Resolution res)
{
	return (state->getBinWidthMHz(res));
}

float64_t
Activity::getBinWidthHz(Resolution res)
{
	return (state->getBinWidthHz(res));
}

/**
 * Compute the number of spectra per frame for the specified resolution.
 *
 * Note:\n
 * 	This assumes overlapping spectra for all resolutions.
 */
int32_t
Activity::getSpectraPerFrame(Resolution res_)
{
	int32_t spectra = 1;

	lock();
	switch (res_) {
	case RES_1KHZ:
	case RES_512HZ:
		spectra *= 2;
	case RES_256HZ:
		spectra *= 2;
	case RES_128HZ:
		spectra *= 2;
	case RES_64HZ:
		spectra *= 2;
	case RES_32HZ:
		spectra *= 2;
	case RES_16HZ:
		spectra *= 2;
	case RES_8HZ:
		spectra *= 2;
	case RES_4HZ:
		spectra *= 2;
	case RES_2HZ:
		spectra *= 2;
	case RES_1HZ:
		spectra *= 2;
		break;
	default:
		unlock();
		Fatal(ERR_IR);
		break;
	}
	unlock();
	return (spectra);
}

//
// getSpectrumTime: compute the time between spectra at the specified
//		resolution
//
// Notes:
//		This is used to compute drift rates
//
float64_t
Activity::getSpectrumTime(Resolution res_)
{
	float64_t spectrumTime = getFrameTime();

	lock();
	switch (res_) {
	case RES_1KHZ:
		spectrumTime /= 2;
	case RES_512HZ:
		spectrumTime /= 2;
	case RES_256HZ:
		spectrumTime /= 2;
	case RES_128HZ:
		spectrumTime /= 2;
	case RES_64HZ:
		spectrumTime /= 2;
	case RES_32HZ:
		spectrumTime /= 2;
	case RES_16HZ:
		spectrumTime /= 2;
	case RES_8HZ:
		spectrumTime /= 2;
	case RES_4HZ:
		spectrumTime /= 2;
	case RES_2HZ:
		spectrumTime /= 2;
	case RES_1HZ:
		spectrumTime /= 2;
		break;
	default:
		unlock();
		Fatal(ERR_IR);
		break;
	}
	unlock();
	return (spectrumTime);
}

/**
 * Compute the total number of spectra in the observation for the specified
 * 	resolution.
 */
int32_t
Activity::getSpectra(Resolution res_)
{
	return (frames * getSpectraPerFrame(res_));
}

void
Activity::setBirdieMask(BirdieMask *birdieMask_)
{
	lock();
	if (birdieMask && !birdieMask->decrementUseCount())
		delete birdieMask;
	birdieMask = birdieMask_;
	if (birdieMask)
		birdieMask->incrementUseCount();
	unlock();
}

BirdieMask *
Activity::getBirdieMask()
{
	lock();
	BirdieMask *birdieMask_ = birdieMask;
	unlock();
	return (birdieMask_);
}

void
Activity::setRcvrBirdieMask(BirdieMask *birdieMask_)
{
	lock();
	if (rcvrBirdieMask && !rcvrBirdieMask->decrementUseCount())
		delete rcvrBirdieMask;
	rcvrBirdieMask = birdieMask_;
	if (rcvrBirdieMask)
		rcvrBirdieMask->incrementUseCount();
	unlock();
}

BirdieMask *
Activity::getRcvrBirdieMask()
{
	lock();
	BirdieMask *birdieMask_ = rcvrBirdieMask;
	unlock();
	return (birdieMask_);
}

void
Activity::setPermRfiMask(PermRfiMask *permRfiMask_)
{
	lock();
	if (permRfiMask && !permRfiMask->decrementUseCount())
		delete permRfiMask;
	permRfiMask = permRfiMask_;
	if (permRfiMask)
		permRfiMask->incrementUseCount();
	unlock();
}

PermRfiMask *
Activity::getPermRfiMask()
{
	lock();
	PermRfiMask *permRfiMask_ = permRfiMask;
	unlock();
	return (permRfiMask_);
}

void
Activity::setRecentRfiMask(RecentRfiMask *recentRfiMask_)
{
	lock();
	if (recentRfiMask && !recentRfiMask->decrementUseCount())
		delete recentRfiMask;
	recentRfiMask = recentRfiMask_;
	if (recentRfiMask)
		recentRfiMask->incrementUseCount();
	unlock();
}

RecentRfiMask *
Activity::getRecentRfiMask()
{
	lock();
	RecentRfiMask *recentRfiMask_ = recentRfiMask;
	unlock();
	return (recentRfiMask_);
}

void
Activity::setTestSignalMask(TestSignalMask *testSignalMask_)
{
	lock();
	if (testSignalMask && !testSignalMask->decrementUseCount())
		delete testSignalMask;
	testSignalMask = testSignalMask_;
	if (testSignalMask)
		testSignalMask->incrementUseCount();
	unlock();
}

TestSignalMask *
Activity::getTestSignalMask()
{
	lock();
	TestSignalMask *testSignalMask_ = testSignalMask;
	unlock();
	return (testSignalMask_);
}

void
Activity::setScienceData(const DxScienceDataRequest *scienceData_)
{
	lock();
	params.scienceDataRequest = *scienceData_;
	unlock();
}

void
Activity::initDoneMask()
{
	lock();
	if (args->zxMode())
		doneMask.set(COLLECT_BIT);
	else
		doneMask.set();
	unlock();
}

void
Activity::resetDoneBit(ProcessBit bit_)
{
	lock();
	doneMask.reset(bit_);
	unlock();
}

bool
Activity::testDoneMask()
{
	lock();
	bool any = doneMask.any();
	unlock();
	return (any);
}

uint32_t
Activity::getDoneMask()
{
	lock();
	uint32_t mask = doneMask.to_ulong();
	unlock();
	return (mask);
}

/**
 * Subchannel mask functions.
 */
void
Activity::createSubchannelMask()
{
	subchannelMask.createSubchannelMask(this);
}

void
Activity::getSubchannelMask(SubchannelMaskBitset *mask_)
{
	subchannelMask.getSubchannelMask(mask_);
}

bool
Activity::isSubchannelMasked(int32_t subchannel_)
{
	return (subchannelMask.isSubchannelMasked(subchannel_));

}

bool
Activity::allSubchannelsMasked()
{
	return (subchannelMask.allSubchannelsMasked());
}

/**
 * Functions for requested subchannel mask. (ZX only)
 *
 * Description:\n
 * 	The requested subchannel mask is the list of subchannels which have
 * 	been requested for transmission by the SSE.\n\n
 * Notes:\n
 * 	This mask is used in an inverted sense: when a mask bit is set, we
 * 	will transmit the subchannel complex amplitudes to the SSE.
 */
void
Activity::initRequestedSubchannelMask()
{
	requestMask.init(this);
}

void
Activity::addRequestedSubchannel(const DxScienceData *scienceData)
{
	requestMask.maskSubchannel(scienceData);
}

void
Activity::createRequestedSubchannelMask()
{
	requestMask.init(this);
	if (args->zxMode()) {
		requestMask.applyRecentRfiMask(getRecentRfiMask(), getSkyFreq());
		// if there are no subchannels masked, use the science data request
		if (requestMask.noSubchannelsMasked()) {
			const DxScienceData &scienceData =
					params.scienceDataRequest.scienceData;
			int32_t subchannel = scienceData.subchannel;
			if (scienceData.requestType == REQ_FREQ)
				subchannel = channel.getSubchannel(scienceData.rfFreq);
			if (subchannel < 0 || subchannel >= getUsableSubchannels())
				subchannel = 0;
			requestMask.maskSubchannel(subchannel);
		}
	}
}

bool
Activity::isSubchannelRequested(int32_t subchannel_)
{
	return (requestMask.isSubchannelMasked(subchannel_));
}

/**
 * Functions for detection statistics.
 */
void
Activity::resetDetectionStatistics()
{
	lock();
	memset(&detectionStats, 0, sizeof(detectionStats));
	unlock();
}

void
Activity::setDetectionStatistics(const DetectionStatistics& stats_)
{
	lock();
	detectionStats = stats_;
	unlock();
}

void
Activity::getDetectionStatistics(DetectionStatistics& stats_)
{
	lock();
	stats_ = detectionStats;
	unlock();
}

/**
 * Detection functions.
 */
int32_t
Activity::getCombinedCount(SignalState type_)
{
	int32_t count =  secondaryCandidateList.getCount(type_);
	count +=  primaryCandidateList.getCount(type_);
	return (count);
}

#ifdef notdef
//
// setActivityParams: set the activity parameters
//
// Note:
//		If params_ == 0, then the activity parameters are cleared,
//		and frames is set to 0.
//
void
Activity::setActivityParams(const DxActivityParameters *params_)
{
	lock();
	if (params_) {
		params = *params_;
		Debug(DEBUG_BIRDIE_MASK, params.rcvrSkyFreq, "rcvrSkyFreq");
		// compute and set the number of frames in the activity
		int32_t f = (int32_t) (params.dataCollectionLength / getFrameTime());
		if ((f + 0.5) * getFrameTime() > params.dataCollectionLength)
			--f;
		frames = (f < 0) ? 0 : f;
	}
	else {
		memset(&params, 0, sizeof(params));
		frames = 0;
	}
	unlock();
}
#endif

void
Activity::selectCandidateList(SystemType origin_)
{
	lock();
	switch (origin_) {
	case SECONDARY:
		candidateList = &secondaryCandidateList;
		break;
	default:
		candidateList = &primaryCandidateList;
		break;
	}
	unlock();
}

Error
Activity::addCwCandidate(CwPowerSignal *sig_, SystemType origin_)
{
	if (!sig_) {
		LogError((DxMessageCode) ERR_NSP, getActivityId(), "activity %d",
				getActivityId());
		Fatal(ERR_NSP);
	}

	CwSignal *sig = new CwSignal(sig_);
	addCandidate(sig, origin_);
	return (0);
}

Error
Activity::addPulseCandidate(PulseSignalHeader *sig_, SystemType origin_)
{
	if (!sig_) {
		LogError((DxMessageCode) ERR_NSP, getActivityId(), "activity %d",
				getActivityId());
		Fatal(ERR_NSP);
	}

	PulseSignal *sig = new PulseSignal(sig_);
	Debug(DEBUG_NEVER, (void *) sig, "sig");
	addCandidate(sig, origin_);
	return (0);
}

void
Activity::addCandidate(Signal *sig, SystemType origin_)
{
	lock();
	sig->setOrigin(origin_);
	switch (origin_) {
	case SECONDARY:
		secondaryCandidateList.add(sig);
		break;
	default:
		primaryCandidateList.add(sig);
		break;
	}
	unlock();
}

void
Activity::removeCandidate(Signal *sig_)
{
	candidateList->remove(sig_);
}

void
Activity::removeCandidateFromEitherList(Signal *sig_)
{
	SignalId id = sig_->getSignalId();
	if (primaryCandidateList.find(id))
		primaryCandidateList.remove(sig_);
	else if (secondaryCandidateList.find(id))
		secondaryCandidateList.remove(sig_);
}

void
Activity::removeCandidate(SignalId& signalId_)
{
	candidateList->remove(signalId_);
}

void
Activity::releaseCandidates(SignalState state_)
{
	// if state doesn't matter, release all candidates
	if (state_ == ANY_STATE)
		candidateList->clear();
	else
		candidateList->remove(state_);
}

Error
Activity::addCwSignal(CwPowerSignal *sig_)
{
	if (!sig_) {
		LogError((DxMessageCode) ERR_NSP, getActivityId(), "activity %d",
				getActivityId());
		Fatal(ERR_NSP);
	}

	CwSignal *sig = new CwSignal(sig_);
	signalList.add(sig);
	return (0);
}

Error
Activity::addPulseSignal(PulseSignalHeader *sig_)
{
	if (!sig_) {
		LogError((DxMessageCode) ERR_NSP, getActivityId(), "activity %d",
				getActivityId());
		Fatal(ERR_NSP);
	}

	PulseSignal *sig = new PulseSignal(sig_);
	signalList.add(sig);
	return (0);
}

void
Activity::releaseSignals()
{
	Debug0((int32_t) signalList.size(), "Activity::releaseSignals");
	// release all signals
	signalList.clear();
}

Error
Activity::addCwFollowupSignal(FollowUpCwSignal *sig_)
{
	if (!sig_) {
		LogError((DxMessageCode) ERR_NSP, getActivityId(), "activity %d",
				getActivityId());
		Fatal(ERR_NSP);
	}

	CwFollowupSignal *sig = new CwFollowupSignal(sig_);
	followupList.add(sig);
	return (0);
}

Error
Activity::addPulseFollowupSignal(FollowUpPulseSignal *sig_)
{
	if (!sig_) {
		LogError((DxMessageCode) ERR_NSP, getActivityId(), "activity %d",
				getActivityId());
		Fatal(ERR_NSP);
	}

	PulseFollowupSignal *sig = new PulseFollowupSignal(sig_);
	followupList.add(sig);
	return (0);
}

void
Activity::removeFollowupSignal(Signal *sig_)
{
	followupList.remove(sig_);
}

void
Activity::releaseFollowupSignals()
{
	followupList.clear();
}

void
Activity::releaseBadBands()
{
	cwBadBandList.clear();
	for (int i = 0; i < MAX_RESOLUTIONS; ++i)
		pulseBadBandList[i].clear();
}

//
// allowSecondaryMsg: test whether a secondary message is allowed
//
// Notes:
//		A secondary message is allowed only if the activity has
//		specified processing of secondary candidates
//
bool
Activity::allowSecondaryMsg()
{
	bool allow = false;
	DxOpsBitset operations(params.operations);

	lock();
	allow = operations.test(PROCESS_SECONDARY_CANDIDATES);
	unlock();
	return (allow);
}

Signal *
Activity::getFirstCandidate(SignalState state_)
{
	return (candidateList->getFirst(state_));
}

Signal *
Activity::getNthCandidate(int32_t i)
{
	if (candidateList->size() <= i)
		return (0);
	SigList::iterator pos;
	for (pos = candidateList->begin(); i; --i, ++pos)
		;
	return (*pos);
}

Signal *
Activity::getFirstSignal()
{
	return (signalList.getFirst());
}

Signal *
Activity::getNthSignal(int32_t i)
{
	if (signalList.size() <= i)
		return (0);
	SigList::iterator pos;
	for (pos = signalList.begin(); i; --i, ++pos)
		;
	return (*pos);
}

Signal *
Activity::getNthFollowupSignal(int32_t i)
{
	if (followupList.size() <= i)
		return (0);
	SigList::iterator pos;
	for (pos = followupList.begin(); i; --i, ++pos)
		;
	return (*pos);
}

Signal *
Activity::findCandidate(SignalId& signalId_)
{
	return (candidateList->find(signalId_));
}

Signal *
Activity::findCandidateInEitherList(SignalId& signalId_)
{

	Signal *got = primaryCandidateList.find(signalId_);
#if 0
	Signal *sec = secondaryCandidateList.find(signalId_);
	Debug(DEBUG_ALWAYS, (int32_t) got, "either got primary");
	Debug(DEBUG_ALWAYS, (int32_t) sec, "either got secondary");
	if (!got)
		got = sec;
#else
	if (!got)
		got = secondaryCandidateList.find(signalId_);
#endif
	return got;
}

Signal *
Activity::findCandidateUsingOrigId(SignalId& signalId_)
{
	return (candidateList->findUsingOrigId(signalId_));
}

Signal *
Activity::findSignal(SignalId& signalId_)
{
	return (signalList.find(signalId_));
}

Signal *
Activity::findFollowupSignal(SignalId& signalId_)
{
	return (followupList.find(signalId_));
}

CwBadBandList *
Activity::getCwBadBandList()
{
	return (&cwBadBandList);
}

PulseBadBandList *
Activity::getPulseBadBandList(Resolution res_)
{
	return (&pulseBadBandList[res_]);
}

/**
 * Get the number of bins in a pulse slice at a specified resolution.
 *
 * Description:\n
 * 	Computes the number of bins in a pulse slice.  Pulse detection is
 * 	sliced to minimize search time.
 *
 * @param		res resolution.
 */
int32_t
Activity::getBinsPerPulseSlice(Resolution res)
{
	int32_t bins = BINS_PER_PULSE_SLICE_1HZ;

	lock();
	if (res < RES_1HZ || res > RES_1KHZ)
		Fatal(ERR_IR);
	bins >>= (res - RES_1HZ);
	unlock();
	return (bins);
}

/**
 * Signal and candidate functions.
 */

int32_t
Activity::getCandidateCount(SignalType type_)
{
	return (candidateList->getCount(type_));
}

int32_t
Activity::getCandidateCount(SignalState state_)
{
	return (candidateList->getCount(state_));
}

int32_t
Activity::getCandidatesOverMax()
{
	int32_t count;

	lock();
	count = candidatesOverMax;
	unlock();
	return (count);
}

int32_t
Activity::getSignalCount(SignalType type_)
{
	return (signalList.getCount(type_));
}

int32_t
Activity::getFollowupSignalCount(SignalType type_)
{
	return (followupList.getCount(type_));
}

int32_t
Activity::getBadBandCount()
{
	return (badBandList.getCount(ANY_TYPE));
}

/**
 * Activity List (singleton)
 *
 * Description:
 *	This is the master list of activities, both allocated and free.
*/

ActivityList *ActivityList::instance = 0;

ActivityList *
ActivityList::getInstance(State *state_, int32_t nAct_)
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new ActivityList(state_, nAct_);
	l.unlock();
	return (instance);
}

ActivityList::ActivityList(State *state_, int32_t nAct_): nActivities(nAct_),
		llock("activityListLock")
{
	for (int32_t i = 0; i < nActivities; ++i) {
		Activity *act = new Activity(state_);
		freeList.push_back(act);
	}
	curPos = allocList.begin();
}

ActivityList::~ActivityList()
{
	lock();
	// remove all entries in both the free and allocated list
	for (ActList::iterator p = freeList.begin(); p != freeList.end(); ++p)
		delete *p;
	freeList.clear();

	for (ActList::iterator p = allocList.begin(); p != allocList.end(); ++p)
		delete *p;
	allocList.clear();
	unlock();
}

Activity *
ActivityList::alloc()
{
	Activity *act = 0;

	lock();
	if (!freeList.empty()) {
		act = freeList.front();
		freeList.pop_front();
		allocList.push_back(act);
	}
	unlock();
	return (act);
}

Error
ActivityList::free(Activity *act_)
{
	ActList::iterator pos;
	Activity *act = 0;
	Error err = ERR_NSA;

	lock();
	// the activity must be in the allocated list
	for (pos = allocList.begin(); pos != allocList.end(); ++pos) {
		if (act_ == *pos) {
			act = *pos;
			act->free();
			act->setState(DX_ACT_NONE);
			allocList.erase(pos);
			freeList.push_back(act);
			err = 0;
			break;
		}
	}
	unlock();
	return (err);
}

Activity *
ActivityList::find(int32_t actId_)
{
	ActList::iterator pos;
	Activity *act = 0;

	lock();
	for (pos = allocList.begin(); pos != allocList.end(); ++pos) {
		act = *pos;
		if (actId_ == act->getActivityId())
			break;
		act = 0;
	}
	unlock();
	return (act);
}

Activity *
ActivityList::getFirst()
{
	Activity *act = 0;

	lock();
	curPos = allocList.begin();
	if (curPos != allocList.end())
		act = *curPos;
	unlock();
	return (act);
}

Activity *
ActivityList::getNext()
{
	Activity *act = 0;

	lock();
	if (curPos != allocList.end() && ++curPos != allocList.end())
		act = *curPos;
	unlock();
	return (act);
}

}
