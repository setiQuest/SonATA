/*******************************************************************************

 File:    Activity.h
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
// $Header: /home/cvs/nss/sonata-pkg/dx/include/Activity.h,v 1.8 2009/03/06 21:45:11 kes Exp $
//
#ifndef _ActivityH
#define _ActivityH

#include <bitset>
#include <sseDxInterface.h>
#include "System.h"
#include "Args.h"
#include "BirdieMask.h"
#include "Channel.h"
#include "CwBadBandList.h"
#include "DxStruct.h"
#include "DxTypes.h"
#include "Lock.h"
#include "PulseBadBandList.h"
#include "PermRfiMask.h"
#include "RecentRfiMask.h"
#include "Sonata.h"
#include "Signal.h"
#include "Spectra.h"
#include "Struct.h"
#include "SubchannelMask.h"
#include "SuperClusterer.h"
#include "TestSignalMask.h"

using namespace sonata_lib;
using spectra::ResData;

namespace dx {

// forward declarations
class State;

// processing steps, used in determining whether an activity
// has been stopped
enum ProcessBit {
	COLLECT_BIT,
	DETECT_BIT,
	CONFIRM_BIT,
	ARCHIVE_BIT,
	NBITS_IN_PROCESS_MASK
};

typedef std::bitset<NBITS_IN_PROCESS_MASK> ProcessMask;

class Activity {
public:
	Activity(State *state_);
	~Activity();

	// set up the activity for an observation
	void setup(const DxActivityParameters& params_);

	// reset the activity by releasing all allocated resources
	void free();

	Channel *getChannel() { return (&channel); }

	float64_t getFrameTime();
	float64_t getFramesPerSec();

	float64_t getChannelWidthMHz();
	float64_t getChannelWidthHz();

	int32_t getUsableSubchannels();
	float64_t getSubchannelWidthMHz();

	int32_t getUsableBinsPerSubchannel(Resolution res);
	int32_t getUsableBinsPerSpectrum(Resolution res);
	float64_t getBinWidthMHz(Resolution res);
	float64_t getBinWidthHz(Resolution res);

	int32_t getSpectraPerFrame(Resolution res_);
	float64_t getSpectrumTime(Resolution res_);
	int32_t getSpectra(Resolution res_);

	void setState(DxActivityState newState_) { activityState = newState_; }
	DxActivityState getState() { return (activityState); }
	void setMode(SystemType mode_) { mode = mode_; }
	SystemType getMode() { return (mode); }
	void setActivityId(int32_t activityId_) { params.activityId = activityId_; }
	int32_t getActivityId() { return (params.activityId); }
	int32_t getChannelNum() { return (params.channelNumber); }
	float64_t getDataCollectionTime() { return (params.dataCollectionLength); }
	uint32_t getFrames() { return (frames); }
	uint32_t getHalfFrames() { return (halfFrames); }
	uint32_t getOperationsMask() { return (params.operations); }
	int32_t getBaselineReportingRate() {
		return (params.scienceDataRequest.baselineReportingHalfFrames);
	}
	int32_t getBaselineHalfFrames() {
		return (params.baselineInitAccumHalfFrames);
	}
	void setSkyFreq(float64_t freq) { params.dxSkyFreq = freq; }
	float64_t getSkyFreq() { return (params.dxSkyFreq); }
	float64_t getIfcSkyFreq() { return (params.ifcSkyFreq); }
	float64_t getRcvrSkyFreq() { return (params.rcvrSkyFreq); }
	const DxActivityParameters& getActivityParams() { return (params); }
	void setObsData(const ObsData& obsData_) { obsData = obsData_; }
	void getObsData(ObsData& obsData_) { obsData_ = obsData; }

	void setBirdieMask(BirdieMask *birdieMask_);
	BirdieMask *getBirdieMask();
	void setRcvrBirdieMask(BirdieMask *birdieMask_);
	BirdieMask *getRcvrBirdieMask();
	void setPermRfiMask(PermRfiMask *permRFIMask_);
	PermRfiMask *getPermRfiMask();
	void setRecentRfiMask(RecentRfiMask *recentRfiMask_);
	RecentRfiMask *getRecentRfiMask();
	void setTestSignalMask(TestSignalMask *testSignalMask_);
	TestSignalMask *getTestSignalMask();
	void setScienceData(const DxScienceDataRequest *scienceData_);
	const DxScienceDataRequest *getScienceData() {
		return (&params.scienceDataRequest);
	}

	// activity parameter access functions
	int32_t getResolutions() { return (resolutions); }
	const ResData *getResData() { return (resData); }
	const ResData *getResData(int32_t i) { return (&resData[i]); }

	void setStartTime(const NssDate& startTime_) {
		startTime = startTime_;
	}
	void setActualStartTime(const NssDate& startTime_) {
		actualStartTime = startTime_;
	}
	const NssDate& getStartTime() { return (startTime); }
	const NssDate& getActualStartTime() { return (actualStartTime); }
	void initDoneMask();
	void resetDoneBit(ProcessBit bit_);
	bool testDoneMask();
	uint32_t getDoneMask();
#ifdef notdef
	void initStopMask();
	void resetStopBit(ProcessBit bit_);
	bool testStopMask();
#endif

	void createSubchannelMask();
	void getSubchannelMask(SubchannelMaskBitset *mask_);
	bool isSubchannelMasked(int32_t subchannel_);
	bool allSubchannelsMasked();

	void setRequestedSubchannelCount(int32_t count) {
		initRequestedSubchannelMask();
		requestedSubchannels = count;
	}
	int32_t getRequestedSubchannelCount() { return (requestedSubchannels); }
	int32_t getRequestedSubchannels() { return (requestMask.getCount()); }
	void initRequestedSubchannelMask();
	void addRequestedSubchannel(const DxScienceData *scienceData);
	void createRequestedSubchannelMask();
	bool isSubchannelRequested(int32_t subchannel_);

	// detection functions
	void resetDetectionStatistics();
	void setDetectionStatistics(const DetectionStatistics& stats_);
	void getDetectionStatistics(DetectionStatistics& stats_);

	void setFollowupSignals(int32_t count) { followupSignals = count; }

	void selectCandidateList(SystemType origin_);

	Error addCwCandidate(CwPowerSignal *sig_, SystemType origin_);
	Error addPulseCandidate(PulseSignalHeader *sig_, SystemType origin_);
	void addCandidate(Signal *sig, SystemType origin_);
	Error addCwSignal(CwPowerSignal *sig_);
	Error addPulseSignal(PulseSignalHeader *sig_);
	Error addCwFollowupSignal(FollowUpCwSignal *sig_);
	Error addPulseFollowupSignal(FollowUpPulseSignal *sig_);

	void setCandidatesOverMax(int32_t candidates_ = 0)
	{
		candidatesOverMax = candidates_;
	}

	void removeCandidate(Signal *sig_);
	void removeCandidateFromEitherList(Signal *sig_);
	void removeCandidate(SignalId& signalId_);
	void releaseCandidates(SignalState state_ = ANY_STATE);
	void releaseSignals();
	void removeFollowupSignal(Signal *sig_);
	void releaseFollowupSignals();
	void releaseBadBands();

	bool allowSecondaryMsg();
	Signal *getFirstCandidate(SignalState state_ = ANY_STATE);
	Signal *getNthCandidate(int32_t i);
	Signal *getFirstSignal();
	Signal *getNthSignal(int32_t i);
	Signal *getNthFollowupSignal(int32_t i);
	Signal *findCandidate(SignalId& signalId_);
	Signal *findCandidateUsingOrigId(SignalId& signalId_);
	Signal *findCandidateInEitherList(SignalId& signalId_);
	Signal *findSignal(SignalId& signalId_);
	Signal *findFollowupSignal(SignalId& signalId_);
	CwBadBandList *getCwBadBandList();
	PulseBadBandList *getPulseBadBandList(Resolution res_);

	int32_t getBinsPerPulseSlice(Resolution res);

	// candidate functions
	int32_t getCandidateCount(SignalType type_ = ANY_TYPE);
	int32_t getCombinedCount(SignalState state_ = ANY_STATE);
	int32_t getCandidateCount(SignalState state_ = ANY_STATE);
	int32_t getCandidatesOverMax();
	int32_t getSignalCount(SignalType type_ = ANY_TYPE);
	int32_t getFollowupSignalCount(SignalType type_ = ANY_TYPE);
	int32_t getFollowupSignals() { return (followupSignals); }
	int32_t getBadBandCount();

	SuperClusterer *getSuperClusterer() { return (superClusterer); }

private:
	int32_t resolutions;				// # of active resolutions
	int32_t candidatesOverMax;			// # of candidates over maximum
	int32_t requestedSubchannels;		// # of subchannels to be requested
	int32_t followupSignals;
	uint32_t frames;					// # of frames
	uint32_t halfFrames;				// # of half frames
	Channel channel;					// channel for this activity
	DetectionStatistics detectionStats;
	ActivityType type;				// normal or baseline
	Lock aLock;
//	Log *dataLog;
//	Log *sseLog;
	SystemType mode;
	DxActivityState activityState;
	DxActivityParameters params;
	BirdieMask *birdieMask;
	BirdieMask *rcvrBirdieMask;
	RecentRfiMask *recentRfiMask;
	PermRfiMask *permRfiMask;
	TestSignalMask *testSignalMask;
	CwBadBandList cwBadBandList;
	NssDate startTime;
	NssDate actualStartTime;
	PulseBadBandList pulseBadBandList[MAX_RESOLUTIONS];
	ProcessMask doneMask;
	ProcessMask stopMask;
	ResData resData[MAX_RESOLUTIONS];
	SignalList *candidateList;
	SignalList primaryCandidateList;
	SignalList secondaryCandidateList;
	SignalList followupList;
	SignalList signalList;
	SignalList badBandList;
	SubchannelMask subchannelMask;
	SubchannelMask requestMask;
	ObsData obsData;
	SuperClusterer *superClusterer;

	Args *args;
	State *state;

	void lock() { aLock.lock(); }
	void unlock() { aLock.unlock(); }

	// forbidden
	Activity(const Activity&);
	Activity& operator=(const Activity&);
};

/**
 * Activity List
 *
 * Description:
 * 	This is a singleton class which maintains the lists of activities,
 * 	both allocated and free.
*/

typedef std::list<Activity *> ActList;

class ActivityList {
public:
	static ActivityList *getInstance(State *state_, int nAct_ = MAX_ACTIVITIES);
	~ActivityList();

	Activity *alloc();
	Error free(Activity *act_);
	Activity *find(int32_t actId_);

	Activity *getFirst();
	Activity *getNext();

	int32_t getFreeCount() { return (freeList.size()); }
	int32_t getAllocCount() { return (allocList.size()); }

private:
	static ActivityList *instance;

	int32_t nActivities;
	ActList freeList;
	ActList allocList;
	ActList::iterator curPos;
//	Activity *actArray;
	Lock llock;

	void lock() { llock.lock(); }
	void unlock() { llock.unlock(); }

	ActList::iterator find(Activity *act_);

	ActivityList(State *_, int nAct_);
};

}

#endif
