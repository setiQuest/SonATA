/*******************************************************************************

 File:    ControlTask.cpp
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

// Control task
//
// This task operates as the system switchboard for ongoing
// activities.  It communicates with the data collection,
// signal detection and confirmation tasks to monitor and
// control system operation.
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/ControlTask.cpp,v 1.12 2009/06/16 15:12:07 kes Exp $
//
#include <iostream>
#include <sseDxInterface.h>
#include <DxOpsBitset.h>
#include "ChildClusterer.h"
#include "ControlTask.h"
#include "CwClusterer.h"
#include "CwFollowupSignal.h"
#include "CwSignal.h"
#include "DxErr.h"
#include "WorkerTask.h"
#include "PulseFollowupSignal.h"
#include "PulseSignal.h"
#include "Util.h"

using std::cout;
using std::endl;

namespace dx {

ControlTask *ControlTask::instance = 0;

ControlTask *
ControlTask::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new ControlTask("ControlTask");
	l.unlock();
	return (instance);
}

ControlTask::ControlTask(string name_): QTask(name_, CONTROL_PRIO),
		archiveQ(0), collectionQ(0), confirmationQ(0), detectionQ(0),
		respQ(0), cmdArgs(0), msgList(0), partitionSet(0),
		state(0), archiveTask(0), collectionTask(0), confirmationTask(0),
		detectionTask(0)
{
}

ControlTask::~ControlTask()
{
}

/**
 * Extract the task arguments and perform initialization.
 */
void
ControlTask::extractArgs()
{
#if ASSIGN_CPUS
	// assign the task processor affinity in multiprocessor systems
	int32_t nCpus = sysconf(_SC_NPROCESSORS_CONF);
	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	int32_t n = 0;
	if (nCpus > 2) {
		// remove affinity for cpu 1
		++n;
	}
	if (nCpus > 3) {
		// remove affinity for cpu 2
		++n;
	}
	// assign affinity
	for (int32_t i = n; i < nCpus; ++i)
		CPU_SET(i, &affinity);
	pid_t tid = gettid();
	int rval = sched_setaffinity(tid, sizeof(cpu_set_t), &affinity);
	Assert(rval >= 0);
#endif

	ControlArgs *controlArgs = static_cast<ControlArgs *> (args);
	Assert(controlArgs);
	respQ = controlArgs->respQ;
	Assert(respQ);

	cmdArgs = Args::getInstance();
	Assert(cmdArgs);
	msgList = MsgList::getInstance();
	Assert(msgList);
	partitionSet = PartitionSet::getInstance();
	Assert(partitionSet);
	state = State::getInstance();
	Assert(state);
#ifdef notdef
	superClusterer = SuperClusterer::getInstance();
	Assert(superClusterer);
#endif

	// create and start the top-level collection, detection, confirmation and
	// archiving tasks.
	createTasks();
	startTasks();
}

/**
 * Create tasks which report to the control task.
 */
void
ControlTask::createTasks()
{
	collectionTask = CollectionTask::getInstance();
	Assert(collectionTask);
	collectionQ = collectionTask->getInputQueue();
	Assert(collectionQ);

	detectionTask = DetectionTask::getInstance();
	Assert(detectionTask);
	detectionQ = detectionTask->getInputQueue();
	Assert(detectionQ);

	confirmationTask = ConfirmationTask::getInstance();
	Assert(confirmationTask);
	confirmationQ = confirmationTask->getInputQueue();
	Assert(confirmationQ);

	archiveTask = ArchiveTask::getInstance();
	Assert(archiveTask);
	archiveQ = archiveTask->getInputQueue();
	Assert(archiveQ);
}

/**
 * Start tasks which report to the control task.
 */
void
ControlTask::startTasks()
{
	// start the collection task
	collectionArgs = CollectionArgs(getInputQueue());
	collectionTask->start(&collectionArgs);

	// start the detection task
	detectionArgs = DetectionArgs(getInputQueue());
	detectionTask->start(&detectionArgs);

	// start the confirmation task
	confirmationArgs = ConfirmationArgs(getInputQueue(), respQ, archiveQ);
	confirmationTask->start(&confirmationArgs);

	// start the archive task
	archiveArgs = ArchiveArgs(getInputQueue());
	archiveTask->start(&archiveArgs);
}

//
// handleMsg: perform control processing
//
// Notes:
//		Many input messages are simply forwarded to the SSE.
//		The control task maintains a stateless condition as
//		much as possible, because there may be several activities
//		active in the system at once.
//
void
ControlTask::handleMsg(Msg *msg)
{
	Debug(DEBUG_CONTROL, (int32_t) msg->getCode(), "code");
	Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(), "act");

	switch (msg->getCode()) {
	case SEND_DX_ACTIVITY_PARAMETERS:
		defineActivity(msg);
		break;
	case DX_TUNED:
		sendTuned(msg);
		break;
	case BEGIN_SENDING_REQUESTED_COMPAMP_SUBCHANNELS:
		startRequestedSubchannels(msg);
		break;
	case SEND_REQUESTED_COMPAMP_SUBCHANNEL:
		addRequestedSubchannel(msg);
		break;
	case DONE_SENDING_REQUESTED_COMPAMP_SUBCHANNELS:
		endRequestedSubchannels(msg);
		break;
	case BEGIN_SENDING_FOLLOW_UP_SIGNALS:
		startFollowupSignals(msg);
		break;
	case SEND_FOLLOW_UP_CW_SIGNAL:
		addCwFollowupSignal(msg);
		break;
	case SEND_FOLLOW_UP_PULSE_SIGNAL:
		addPulseFollowupSignal(msg);
		break;
	case DONE_SENDING_FOLLOW_UP_SIGNALS:
		endFollowupSignals(msg);
		break;
	case START_TIME:
		startActivity(msg);
		break;
	case BASELINE_INIT_ACCUM_STARTED:
//		cout << "baseline started" << endl;
		sendCollectionStarted(msg);
		break;
	case DATA_COLLECTION_STARTED:
//		cout << "data collection started" << endl;
		sendCollectionStarted(msg);
		break;
	case BASELINE_INIT_ACCUM_COMPLETE:
		break;
	case DATA_COLLECTION_COMPLETE:
		Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(),
				"DATA_COLLECTION_COMPLETE, act");
		sendCollectionComplete(msg);
		if (cmdArgs->zxMode())
			sendActivityComplete(msg);
		else
			startDetection(msg);
		break;
	case SIGNAL_DETECTION_STARTED:
		Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(),
				"SIGNAL_DETECTION_STARTED, act");
		sendDetectionStarted(msg);
		break;
	case DetectionComplete:
		Debug(DEBUG_CONTROL, 0, "DetectionComplete");

		Activity *act;

		if (!(act = state->findActivity(msg->getActivityId())))
			break;
		act->resetDoneBit(DETECT_BIT);
		Debug(DEBUG_CONTROL, (int32_t) act->getDoneMask(),
				"DetectionComplete, doneMask");
		sendDetectionResults(msg);
		// start the archiver for the primary detection
		// start confirmation only for the primary detector
		if (act->getMode() == PRIMARY)
			startArchive(msg);
		startConfirmation(msg);
		break;
	case ConfirmationComplete:
		Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(),
				"ConfirmationComplete, act");
		sendConfirmationComplete(msg);
		break;
	case REQUEST_ARCHIVE_DATA:
		Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(),
				"REQUEST_ARCHIVE_DATA, act");
		requestArchive(msg);
		break;
	case DISCARD_ARCHIVE_DATA:
		Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(),
				"DISCARD_ARCHIVE_DATA, act");
		discardArchive(msg);
		break;
	case ArchiveComplete:
		sendArchiveComplete(msg);
		break;
	case STOP_DX_ACTIVITY:
		Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(),
				"STOP_DX_ACTIVITY, act");
		stopActivity(msg);
		break;
	case ActivityStopped:
		testActivityStopped(msg);
		break;
	default:
		// if we're processing secondary candidates, we have to handle
		// additional messages
		Error err = handleSecondaryMsg(msg);
		if (err) {
			LogFatal(err, msg->getActivityId(), "code %d", msg->getCode());
			ErrStr(msg->getCode(), "msg code");
			ErrStr(msg->getUnit(), "unit");
			Fatal(ERR_IMT);
		}
		break;
	}
}

//
// handleSecondaryMsg: handle additional messages for a secondary detection
//
// Notes:
//		These messages are allowed only when the activity specifies the
//		DX will be processing secondary candidates.
//		The secondary must handle signal descriptions from the primary detector;
//		these signal descriptions specify the candidate signals to be
//		confirmed.
//
Error
ControlTask::handleSecondaryMsg(Msg *msg)
{
	// find the activity
	Activity *act;
	if (!(act = state->findActivity(msg->getActivityId()))) {
		LogError(ERR_NSA, msg->getActivityId(), "message = %d",
				msg->getCode());
		return (ERR_NSA);
	}

	// get the activity parameters and make sure this is a secondary detection
	if (!act->allowSecondaryMsg())
		return (ERR_IMT);

	Error err = 0;
	switch (msg->getCode()) {
	case BEGIN_SENDING_CANDIDATES:
	{
		Count *count = static_cast<Count *> (msg->getData());
		Debug(DEBUG_CONTROL, count->count,"BEGIN_SENDING_CANDIDATES, count");
		// start the secondary confirmation process
		// startSecondaryConfirmation(msg);
	}
		break;
	case SEND_CANDIDATE_CW_POWER_SIGNAL:
		Debug(DEBUG_CONTROL, 0, "SEND_CANDIDATE_CW_POWER_SIGNAL");
		err = addCwCandidate(msg, act);
		break;
	case SEND_CANDIDATE_PULSE_SIGNAL:
		err = addPulseCandidate(msg, act);
		break;
	case DONE_SENDING_CANDIDATES:
		Debug(DEBUG_CONTROL, act->getCandidateCount(CW_POWER),
				"DONE_SENDING_CANDIDATES, candidates");
		err = endCandidates(msg, act);
		break;
	case BEGIN_SENDING_CW_COHERENT_SIGNALS:
		Debug(DEBUG_CONTROL, 0, "BEGIN_SENDING_CW_COHERENT_SIGNALS");
		break;
	case SEND_CW_COHERENT_SIGNAL:
		Debug(DEBUG_CONTROL, 0, "SEND_CW_COHERENT");
		err = sendCwCoherentSignal(msg, act);
		break;
	case DONE_SENDING_CW_COHERENT_SIGNALS:
		err = endCwCoherentSignals(msg, act);
		break;
	default:
		err = ERR_IMT;
		break;
	}
	return (err);
}

//
// defineActivity: sets the activity parameters for a new activity
//
// Synopsis:
//		void defineActivity(msg);
//		Msg *msg;				// message
// Returns:
//		Nothing.
// Description:
//		Allocates a new activity and assigns the activity parameters
//		to it, along with the current masks and Doppler.
// Notes:
//		Only one activity may be defined at a time.
//		Masks and Doppler can be changed until the activity is
//		started with a START_TIME command.
//
void
ControlTask::defineActivity(Msg *msg)
{
	SseInterfaceHeader& hdr = msg->getHeader();

	Assert(hdr.dataLength == sizeof(DxActivityParameters));

//	Debug(DEBUG_CONTROL, state->getFreeActivities(), "free activities");

	// make sure there is no activity already defined
	Activity *act = state->findActivity(DX_ACT_INIT);
	if (act) {
		LogError(ERR_AAD, act->getActivityId(),
				"activity: %d; attempt to define activity %d",
				act->getActivityId(), hdr.activityId);
		return;
	}

	DxActivityParameters *params
			= static_cast<DxActivityParameters *> (msg->getData());
	Assert(params);

	/////////////////////////////////////////////////////////////
	// TEST ONLY!!!  Set the single pulse resolution to the same
	// as the DADD resolution
	////////////////////////////////////////////////////////////
	for (int32_t i = 0; i < MAX_RESOLUTIONS; ++i) {
		if (i == params->daddResolution)
			params->requestPulseResolution[i] = SSE_TRUE;
		else if (i < RES_256HZ)
			params->requestPulseResolution[i] = SSE_FALSE;
		else
			params->requestPulseResolution[i] = SSE_FALSE;
	}
//	params->requestPulseResolution[RES_1KHZ] = SSE_TRUE;

	// if we are running cache-efficient DADD, the observation
	// length must be a power of two number of frames, so we must
	// adjust it to the largest power of two which fits.  In any
	// case, warn if number of frames is not a power of two.
	// The computation of the number of frames is complicated by
	// the fact that the data collection length is a half-frame
	// longer than twice the number of frames.
	int32_t frames =
			(uint32_t) (params->dataCollectionLength / state->getFrameTime());
	if ((frames + 0.5) * state->getFrameTime() > params->dataCollectionLength)
		frames--;

	uint32_t actualFrames = frames;
//	if (!isPow2(frames) && cmdArgs->doCacheEfficientDadd()) {
	if (!isPow2(frames)) {
		actualFrames = pow2(frames);
		LogWarning(ERR_NP2, msg->getActivityId(),
				"frames = %d, not a power of two, using %d frames",
				frames, actualFrames);
		frames = actualFrames;
	}
	if (frames > state->getMaxFrames()) {
		LogError(ERR_TMF, msg->getActivityId(),
				"activity %d, frames = %d, max = %d", msg->getActivityId(),
				frames, state->getMaxFrames());
		return;
	}

	// reset the data collection length to the actual length based on the
	// computed number of half frames and rounded up to the next full number
	// of seconds
	params->dataCollectionLength =
			static_cast<int32_t> (ceil(frames * state->getFrameTime()));

	/////////////////////////////////////////////////////////////////
	////// test code to set operations mask
	DxOpsBitset operations(params->operations);
//	perations.set(BASELINING);
//	operations.set(POWER_CWD;
//	operations.reset(POWER_CWD);
//	operations.reset(COHERENT_CWD);
//	operations.reset(PULSE_DETECTION);
//	operations.reset(PROCESS_SECONDARY_CANDIDATES);
//	operations.reset(FOLLOW_UP_CANDIDATES);
	params->operations = operations.to_ulong();

	// allocate an activity and define it
	act = state->allocActivity(params);
	if (!act) {
		LogFatal(ERR_NAA, hdr.activityId, "");
		Fatal(ERR_NAA);
	}

	// tune the detector
	tune(msg);
}

//
// tune: send a message to the collection task, telling it to request
//		DX tuning
//
// Notes:
//		This is sent when the activity is defined.
//		The message is simply forwarded to the collection task
//		for processing.
//
void ControlTask::tune(Msg *msg)
{
	Debug(DEBUG_CONTROL, msg->getActivityId(), "activity id");

	Msg *cMsg = msgList->alloc((DxMessageCode) Tune,
			msg->getActivityId());
	collectionQ->send(cMsg);
}

//
// sendTuned: issue a DX_TUNED message to the SSE
//
// Notes:
//		Tuning is performed by the collection task, which
//		returns this message when tuning is complete.
//
void
ControlTask::sendTuned(Msg *msg)
{
	Debug(DEBUG_CONTROL, msg->getActivityId(), "activity id");

	Msg *resp = msgList->alloc();
	msg->forward(resp);
	respQ->send(resp);
}

/**
 * Start requested complex amplitude subchannels
 */
void
ControlTask::startRequestedSubchannels(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	Activity *act;

	Assert(msg->getDataLength() == sizeof(Count));

	// make sure the activity exists and is ready to start
	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d not defined", activityId);
		return;
	}

	Count *count = static_cast<Count *> (msg->getData());
	act->setRequestedSubchannelCount(count->count);
}

/**
 * Add a requested subchannel
 */
void
ControlTask::addRequestedSubchannel(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	Activity *act;

	Assert(msg->getDataLength() == sizeof(DxScienceData));

	// make sure the activity exists and is ready to start
	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d not defined", activityId);
		return;
	}
	DxScienceData *scienceData = static_cast<DxScienceData *> (msg->getData());
	act->addRequestedSubchannel(scienceData);
}

/**
 * End requested subchannels
 */
void
ControlTask::endRequestedSubchannels(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	Activity *act;

	// make sure the activity exists and is ready to start
	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d not defined", activityId);
		return;
	}

	if (act->getRequestedSubchannels() != act->getRequestedSubchannelCount()) {
		LogWarning(ERR_ISC, activityId,
			"specified count = %d, actual = %d",
			act->getRequestedSubchannelCount(),
			act->getRequestedSubchannels());
	}
}
//
// startFollowupSignals: begin list of followup signals
//
// Notes:
//		Followup signals are sent only to the primary.
//		A list of signals is created against which all
//		signals detected will be checked.
//
void
ControlTask::startFollowupSignals(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	Count *count;
	Activity *act;

	Assert(msg->getDataLength() == sizeof(Count));

	// make sure the activity exists and is ready to start
	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d not defined", activityId);
		return;
	}

	count = static_cast<Count *> (msg->getData());
	act->setFollowupSignals(count->count);
}

//
// addCwFollowupSignal: add a Cw followup signal to the followup
//		signal list
//
// Notes:
//
void
ControlTask::addCwFollowupSignal(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	FollowUpCwSignal *signal;
	Activity *act;

	Assert(msg->getDataLength() == sizeof(FollowUpCwSignal));

	// make sure the activity exists and is ready to start
	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d not defined", activityId);
		return;
	}

	signal = static_cast<FollowUpCwSignal *> (msg->getData());
	Debug(DEBUG_CONTROL, signal->sig.rfFreq, "rfFreq");
	act->addCwFollowupSignal(signal);
}

//
// addFollowupPulseSignal: add a pulse followup signal to the followup
//		signal list
//
// Notes:
//
void
ControlTask::addPulseFollowupSignal(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	FollowUpPulseSignal *signal;
	Activity *act;

	Assert(msg->getDataLength() == sizeof(FollowUpPulseSignal));

	// make sure the activity exists and is ready to start
	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d not defined", activityId);
		return;
	}

	signal = static_cast<FollowUpPulseSignal *> (msg->getData());
	act->addPulseFollowupSignal(signal);
}

//
// endFollowupSignals: log end of followup signals
//
// Notes:
//		The number of followup signals actually sent must be
//		equal to the number specified in the BEGIN message
//		The code will work okay if this message is never sent
//		by the SSE.
//
void
ControlTask::endFollowupSignals(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	Activity *act;

	// make sure the activity exists and is ready to start
	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d not defined", activityId);
		return;
	}

	if (act->getFollowupSignals() != act->getFollowupSignalCount()) {
		LogWarning(ERR_ISC, activityId,
			"specified count = %d, actual = %d", act->getFollowupSignals(),
			act->getFollowupSignalCount());
	}
}

/**
 * Start an activity.
 *
 * Description:\n
 * 	Starts the activity by sending the start time to the collection task.
 * 	First, the activity state is set to pending baseline accumulation.
 */
void
ControlTask::startActivity(Msg *msg)
{
	int32_t activityId = msg->getActivityId();
	Activity *act = state->findActivity(activityId);
	if (!act) {
		LogError(ERR_NSA, activityId, "activity %d", activityId);
		return;
	}
	// set all the masks
	act->setPermRfiMask(state->getPermRfiMask());
	act->setBirdieMask(state->getBirdieMask());
	act->setRcvrBirdieMask(state->getRcvrBirdieMask());
	act->setRecentRfiMask(state->getRecentRfiMask());
	act->setTestSignalMask(state->getTestSignalMask());

#ifdef notdef
	if (cmdArgs->zxMode() && !act->getRequestedSubchannels()) {
		DxScienceData scienceData = act->getScienceData()->scienceData;
		++scienceData.subchannel;
		act->addRequestedSubchannel(&scienceData);
	}
#endif

	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	collectionQ->send(cMsg);
}

//
// stopActivity: stop one or more activities
//
// Notes:
//		Care must be taken to ensure that an activity can be
//		stopped regardless of its state (which may be transitional
//		between one processing task and another).
//		If there is no activity ID specified (NSS_NO_ACTIVITY_ID), then
//		all current activities must be stopped
//
void
ControlTask::stopActivity(Msg *msg)
{
	Debug(DEBUG_CONTROL, (int32_t) msg->getCode(), "code");
	Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(), "act");

	int32_t activityId = msg->getActivityId();
	if (activityId == NSS_NO_ACTIVITY_ID) {
		// stop all activities
		for (Activity *act = state->getFirstActivity(); act;
				act = state->getNextActivity()) {
//			cout << "stop act " << act->getActivityId() << endl;
			stopActivity(msg, act);
		}
	}
	else if (state->findActivity(activityId)) {
		// kill any corresponding baseline activity
		Debug(DEBUG_CONTROL, activityId, "act id");
		Activity *act = state->findActivity(-activityId);
		if (act)
			stopActivity(msg, act);
		// then kill the activity
		act = state->findActivity(activityId);
		stopActivity(msg, act);
	}
}

//
// stopActivity: stop a specific activity
//
void
ControlTask::stopActivity(Msg *msg, Activity *act)
{
	int32_t activityId = act->getActivityId();
	DxActivityState state_;

	Debug(DEBUG_CONTROL, (int32_t) msg->getCode(), "code");
	Debug(DEBUG_CONTROL, (int32_t) act->getActivityId(), "act");
	Debug(DEBUG_CONTROL, (int32_t) act->getState(), "state");

	// this task will stop only activities for which it has
	// current responsibility; otherwise, it will pass the
	// message on to the processing tasks
	switch (state_ = act->getState()) {
	case DX_ACT_NONE:
	case DX_ACT_INIT:
	case DX_ACT_COMPLETE:
	case DX_ACT_STOPPED:
	case DX_ACT_DC_COMPLETE:
	case DX_ACT_SD_COMPLETE:
	{
		// we're responsible for this activity, so notify the SSE
		// that it's done
		MemBlk *blk = partitionSet->alloc(sizeof(DxActivityStatus));
		Assert(blk);
		DxActivityStatus *status = static_cast<DxActivityStatus *>
				(blk->getData());
		status->activityId = activityId;
		status->currentState = DX_ACT_STOPPED;
		Msg *rMsg = msgList->alloc(DX_ACTIVITY_COMPLETE, activityId, status,
				sizeof(DxActivityStatus), blk);
		respQ->send(rMsg);

		// now release the activity
		act->setState(DX_ACT_NONE);
		Debug(DEBUG_CONTROL, act->getActivityId(), "freeActivity");
		state->freeActivity(act);
		break;
	}
	case DX_ACT_TUNED:
	case DX_ACT_PEND_BASE_ACCUM:
	case DX_ACT_RUN_BASE_ACCUM:
	case DX_ACT_PEND_DC:
	case DX_ACT_RUN_DC:
	case DX_ACT_PEND_SD:
	case DX_ACT_RUN_SD:
		stopCollection(msg, activityId);
		stopDetection(msg, activityId);
		break;
	case DX_ACT_ERROR:
	case DX_ACT_STOPPING:
		// if it's already being stopped, don't do anything
		break;
	default:
		LogFatal(ERR_IAS, activityId, "activity %d, state %d", activityId,
				state_);
		Fatal(ERR_IAS);
		break;
	}
}

void
ControlTask::sendCollectionStarted(Msg *msg)
{
	Msg *resp = msgList->alloc();
	msg->forward(resp);
	respQ->send(resp);
}

//
// sendCollectionComplete: notify the SSE that data collection is
//		complete
//
void
ControlTask::sendCollectionComplete(Msg *msg)
{
	Activity *act = state->findActivity(msg->getActivityId());
	if (act)
		act->resetDoneBit(COLLECT_BIT);
	Debug(DEBUG_CONTROL, act->getActivityId(), "act id");
	Debug(DEBUG_CONTROL, (int32_t) act->getDoneMask(), "doneMask");

	Msg *resp = msgList->alloc((DxMessageCode) msg->getCode(),
			msg->getActivityId());
	respQ->send(resp);
}

//
// startDetection: start the detection process
//
// Description:
//		Initiates detection by sending a message to the
//		detection task.
// Notes:
//		If data collection is completed before detection
//		of the previous observation is completed, detection
//		of the succeeding task will be queued until
//		detection can be started.
//
void
ControlTask::startDetection(Msg *msg)
{
	Activity *act;

//	cout << "start detection " << msg->getActivityId() << endl;

	if (!(act = state->findActivity(msg->getActivityId())))
		return;

	// notify the SSE of the change of state; if there was
	// a problem, we're done with the activity
	if (act->getState() != DX_ACT_DC_COMPLETE) {
		sendActivityComplete(msg);
		return;
	}

	// set the activity to pending signal detection; if there
	// is any other activity in signal detection or pending
	// signal detection, delay the message until the other
	// activity has completed.
	if (!state->findActivity(DX_ACT_RUN_SD)
			&& !state->findActivity(DX_ACT_PEND_SD)) {
		act->setState(DX_ACT_PEND_SD);
		Msg *dMsg = msgList->alloc((DxMessageCode) StartDetection,
				msg->getActivityId());
		detectionQ->send(dMsg);
	}
	else
		act->setState(DX_ACT_PEND_SD);
}

void
ControlTask::sendDetectionStarted(Msg *msg)
{
	Msg *resp = msgList->alloc();
	msg->forward(resp);
	respQ->send(resp);
}

//
// sendDetectionResults: cluster the signals and send them
//		and the candidates to the SSE
//
// Description:
//		Performs clustering as required, selects candidate
//		signals
// Notes:
//		For a secondary detection, there is no candidate selection
//
void
ControlTask::sendDetectionResults(Msg *msg)
{
	int32_t activityId = msg->getActivityId();

	clusterSignals(activityId);
	classifySignals(activityId);
	sendCandidates(activityId);
	sendSignals(activityId);
	sendBadBands(activityId);
}

void
ControlTask::startConfirmation(Msg *msg)
{
	Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(), "startConf");

	// notify the confirmation task that it is time to process
	// candidates
	Msg *cMsg = msgList->alloc((DxMessageCode) StartConfirmation,
			msg->getActivityId());
	confirmationQ->send(cMsg);
}

//
// startArchive: start the archive process
//
// Notes:
//		Notifies the archive task that archiving is to begin for the
//		specified activity.
//
void
ControlTask::startArchive(Msg *msg)
{
	Debug(DEBUG_CONTROL, 0, 0);

	// notify the archive task that it is time to process
	// candidates
	Msg *aMsg = msgList->alloc((DxMessageCode) StartArchive,
			msg->getActivityId());
	Debug(DEBUG_ARCHIVE, msg->getActivityId(), " startArchive ");
	archiveQ->send(aMsg);
}


//
// sendConfirmationComplete: notify the archive task that
//		the last candidate has been processed
//
void
ControlTask::sendConfirmationComplete(Msg *msg)
{
	Activity *act = state->findActivity(msg->getActivityId());
	if (!act)
		return;

	act->resetDoneBit(CONFIRM_BIT);

	Debug(DEBUG_CONTROL, (int32_t) act->getDoneMask(), "doneMask");
	Debug(DEBUG_ARCHIVE, (int32_t) act->getDoneMask(), "doneMask");

	// if we're currently a primary and this is a multibeam activity,
	// we're not done until the other beam candidates are processed
	if (act->getMode() == PRIMARY && act->allowSecondaryMsg()){
		act->setMode(SECONDARY);
		act->selectCandidateList(SECONDARY);
		// reset the done mask, but don't expect data collection or
		// signal detection
		act->initDoneMask();
		act->resetDoneBit(COLLECT_BIT);
		act->resetDoneBit(DETECT_BIT);
		Debug(DEBUG_CONTROL, (int32_t) act->getDoneMask(), "doneMask");
	}
	else {
		// we're really done; notify the archive task that all candidates
		// have been processed
		Debug(DEBUG_CONFIRM, (int32_t) act->testDoneMask(), "secondary conf complete");
		Debug(DEBUG_ARCHIVE, (int32_t) act->testDoneMask(), "secondary conf complete");
		if (act->testDoneMask()) {
		    // Reset the Candidate list to the Primary Candidates
			// for the Archiver
		    act->selectCandidateList(PRIMARY);
			Msg *aMsg = msgList->alloc();
			msg->forward(aMsg);
			archiveQ->send(aMsg);
			if (act->allowSecondaryMsg()) {
				aMsg = msgList->alloc(DONE_SENDING_CANDIDATE_RESULTS,
								msg->getActivityId());
				respQ->send(aMsg);
			}
		}
		else
			sendActivityComplete(msg);
	}
}

//
// requestArchive: request archiving of a candidate
//
void
ControlTask::requestArchive(Msg *msg)
{
	Debug(DEBUG_CONTROL, msg->getActivityId(), "act");
	Debug(DEBUG_ARCHIVE, msg->getActivityId(), "act req arch ");
	Msg *aMsg = msgList->alloc();
	msg->forward(aMsg);
	archiveQ->send(aMsg);
}

//
// discardArchive: discard archive data for a candidate
//
void
ControlTask::discardArchive(Msg *msg)
{
	Msg *aMsg = msgList->alloc();
	msg->forward(aMsg);
	Debug(DEBUG_ARCHIVE, msg->getActivityId(), "act req discard ");
	archiveQ->send(aMsg);
}

//
// sendArchiveComplete: notify the archive task that
//		the last candidate has been processed
//
void
ControlTask::sendArchiveComplete(Msg *msg)
{

	Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(), "act");
	Activity *act = state->findActivity(msg->getActivityId());
	if (act) {
		Msg *rMsg = msgList->alloc(ARCHIVE_COMPLETE, msg->getActivityId());
		respQ->send(rMsg);
		act->resetDoneBit(ARCHIVE_BIT);
		Debug(DEBUG_CONTROL, (int32_t) act->getDoneMask(), "doneMask");
	}
	sendActivityComplete(msg);
}

void
ControlTask::stopCollection(Msg *msg, int32_t activityId)
{
	Msg *cMsg = msgList->alloc(STOP_DX_ACTIVITY, activityId);
	collectionQ->send(cMsg);
}

//
// stop signal detection
//
// Notes:
//		We don't know whether we're in signal detection,
//		confirmation, or archiving, so send messages to
//		all of them
//
void
ControlTask::stopDetection(Msg *msg, int32_t activityId)
{
	Debug(DEBUG_CONTROL, activityId, "stopping act");

	Msg *dMsg = msgList->alloc(STOP_DX_ACTIVITY, activityId);
	detectionQ->send(dMsg);

	Msg *cMsg = msgList->alloc(STOP_DX_ACTIVITY, activityId);
	confirmationQ->send(cMsg);

	Msg *aMsg = msgList->alloc(STOP_DX_ACTIVITY, activityId);
	archiveQ->send(aMsg);
}

//
// testActivityStopped: log the stop for this activity; if
//		all units have reported, we are done
//
void
ControlTask::testActivityStopped(Msg *msg)
{
	Activity *act;
	if (!(act = state->findActivity(msg->getActivityId())))
		return;

	// see whether there are any bits set
	Debug(DEBUG_CONTROL, (int32_t) msg->getActivityId(), "act");
	Debug(DEBUG_CONTROL, (int32_t) act->testDoneMask(), "done mask");
	if (act->testDoneMask()) {
		switch (static_cast<dx::Unit> (msg->getUnit())) {
		case UnitCollect:
			Debug(DEBUG_CONTROL, msg->getUnit(), "collect");
			act->resetDoneBit(COLLECT_BIT);
			break;
		case UnitDetect:
			Debug(DEBUG_CONTROL, msg->getUnit(), "detect");
			act->resetDoneBit(DETECT_BIT);
			break;
		case UnitConfirm:
			Debug(DEBUG_CONTROL, msg->getUnit(), "confirm");
			act->resetDoneBit(CONFIRM_BIT);
			break;
		case UnitArchive:
			Debug(DEBUG_CONTROL, msg->getUnit(), "archive");
			act->resetDoneBit(ARCHIVE_BIT);
			break;
		default:
			break;
		}
		Debug(DEBUG_CONTROL, (int32_t) act->getDoneMask(), "done mask");
		if (!act->testDoneMask()) {
			Debug(DEBUG_CONTROL, msg->getActivityId(), "stopped act");
			act->setState(DX_ACT_STOPPED);
			sendActivityComplete(msg);
		}
	}
}

//
// sendActivityComplete
//
// Notes:
//		This message will be sent in the case of an activity
//		which was stopped as well as one which completed.
//
void
ControlTask::sendActivityComplete(Msg *msg)
{
	int32_t activityId = msg->getActivityId();

	// make sure nobody else has released this structure
	Activity *act = state->findActivity(activityId);
	if (!act)
		return;

	Debug(DEBUG_CONTROL, (int32_t) act->getDoneMask(), "doneMask");

	// make sure all components have finished
	Debug(DEBUG_CONTROL, (int32_t) act->testDoneMask(), "test doneMask");
	if (act->testDoneMask())
		return;
	act->setState(DX_ACT_COMPLETE);

	// send a signal detection complete followed by an activity
	// complete
	Msg *rMsg = msgList->alloc(SIGNAL_DETECTION_COMPLETE, activityId);
	respQ->send(rMsg);

	// send the activity complete message
	MemBlk *blk = partitionSet->alloc(sizeof(DxActivityStatus));
	Debug(DEBUG_CONTROL, (void *) blk, "blk");
	Assert(blk);
	DxActivityStatus *status = static_cast<DxActivityStatus *>
			(blk->getData());
	status->activityId = activityId;
	status->currentState = act->getState();
	rMsg = msgList->alloc(DX_ACTIVITY_COMPLETE, act->getActivityId(),
			status, sizeof(DxActivityStatus), blk);
	respQ->send(rMsg);

	// now release the activity
	act->setState(DX_ACT_NONE);
	Debug(DEBUG_CONTROL, act->getActivityId(), "freeActivity");
	state->freeActivity(act);

	act = state->findActivity(activityId);
	Debug(DEBUG_CONTROL, activityId, "act");

	// if there is an activity pending signal detection, start it
	// now
	act = state->findActivity(DX_ACT_PEND_SD);
	if (act) {
		Msg *dMsg = msgList->alloc((DxMessageCode) StartDetection,
				act->getActivityId());
		detectionQ->send(dMsg);
	}

#ifdef notdef
	// debugging: count the number of blocks in each free list
	partitionSet->countFreeBlks();
#endif

	// display the # of msg allocs and frees
	Debug(DEBUG_CONTROL, msgList->getAllocs(), "allocs");
	Debug(DEBUG_CONTROL, msgList->getFrees(), "frees");
}

//
// clusterSignals: cluster the signals
//
void
ControlTask::clusterSignals(int32_t activityId)
{
	Activity *act;

	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d", activityId);
		return;
	}

	// do the signal clustering
	SuperClusterer *superClusterer = act->getSuperClusterer();
	Assert(superClusterer);
	superClusterer->compute();
	Debug(DEBUG_NEVER, superClusterer->getCount(), "supercluster count");
}

//
// classifySignals: assign a classification to signals
//
void
ControlTask::classifySignals(int32_t activityId)
{
	Activity *act;

	if (!(act = state->findActivity(activityId))) {
		LogError(ERR_NSA, activityId, "activity %d ", activityId);
		return;
	}

	// classify the signals
	classifier.classifySignals(act);
}

//
// sendCandidates: send the candidate signals
//
// Notes:
//		This will occur after primary processing and before
//		secondary candidate processing.
//
void
ControlTask::sendCandidates(int32_t activityId)
{
	Activity *act;

	if (!(act = state->findActivity(activityId)))
		return;

	// get the activity parameters
	const DxActivityParameters& params = act->getActivityParams();

	// if candidates are not being selected, don't send any
	// messages
	DxOpsBitset operations = params.operations;
//	cout << "candidate selection = " << operations.test(CANDIDATE_SELECTION);
//	cout << endl;
	if (!operations.test(CANDIDATE_SELECTION))
		return;

	// record the total signal count, but no more than maximum
	// candidates allowed
	int32_t candidates = act->getCandidateCount(ANY_TYPE);
	int32_t followupSignals = act->getFollowupSignalCount();

	// send the signal count
	MemBlk *blk = partitionSet->alloc(sizeof(Count));
	Assert(blk);
	Count *count = static_cast<Count *> (blk->getData());
	count->count = candidates + followupSignals;
	Msg *msg = msgList->alloc(BEGIN_SENDING_CANDIDATES, activityId, count,
			sizeof(Count), blk);
	respQ->send(msg);

	// send the candidates
	Debug(DEBUG_NEVER, candidates, "sending candidates");
	for (int32_t i = 0; i < candidates; i++) {
		Signal *signal = act->getNthCandidate(i);
		CwSignal *cwSignal = signal->getCw();
		PulseSignal *pulseSignal = signal->getPulse();
		if (cwSignal) {
			Debug(DEBUG_NEVER, i, "cwd signal");
			blk = partitionSet->alloc(sizeof(CwPowerSignal));
			Assert(blk);
			::CwPowerSignal *cwSig
					= static_cast< ::CwPowerSignal *> (blk->getData());
			*cwSig = cwSignal->getSignal();
#ifdef notdef
			cwSig->sig.subchannelNumber = act->getSubchannel(cwSig->sig.rfFreq);
			cwSig->sig.pol = superClusterer->getNthPolarization(idx);
			cwSig->sig.signalId = superClusterer->getNthSignalId(idx);
#endif
			msg = msgList->alloc(SEND_CANDIDATE_CW_POWER_SIGNAL, activityId,
					cwSig, sizeof(::CwPowerSignal), blk);
			respQ->send(msg);
		}
		else if (pulseSignal) {
			Debug(DEBUG_NEVER, i, "pd candidate");
			PulseSignalHeader *sig = pulseSignal->getSignal();
			Debug(DEBUG_NEVER, (void *) sig, "pulse signal");
			Debug(DEBUG_NEVER, sig->train.numberOfPulses, "pulses");
			size_t len = sizeof(PulseSignalHeader)
					+ sig->train.numberOfPulses * sizeof(::Pulse);
			blk = partitionSet->alloc(len);
			Assert(blk);
			PulseSignalHeader *pSig
					= static_cast<PulseSignalHeader *> (blk->getData());
			*pSig = *sig;
			::Pulse *sp = (::Pulse *) (sig + 1);
			::Pulse *dp = (::Pulse *) (pSig + 1);
			for (int j = 0; j < sig->train.numberOfPulses; ++j) {
//				cout << "pulse " << j << " " << *sp;
				*dp++ = *sp++;
			}
			msg = msgList->alloc(SEND_CANDIDATE_PULSE_SIGNAL, activityId,
					pSig, len, blk);
			respQ->send(msg);
		}
	}

	// send all followup signals which were not confirmed, but send them
	// as candidates.  The followup list now contains only those followup
	// signals which were not found during signal detection.
	for (int32_t i = 0; i < act->getFollowupSignalCount(); ++i) {
		Signal *signal = act->getNthFollowupSignal(i);
		CwFollowupSignal *cwFollowupSignal = signal->getCwFollowup();
		PulseFollowupSignal *pulseFollowupSignal = signal->getPulseFollowup();
		if (cwFollowupSignal) {
			blk = partitionSet->alloc(sizeof(::CwPowerSignal));
			Assert(blk);
			CwPowerSignal *cwSig
					= static_cast<CwPowerSignal *> (blk->getData());

			FollowUpCwSignal followupCwSig = cwFollowupSignal->getSignal();
			cwSig->sig.path.rfFreq = followupCwSig.sig.rfFreq;
			cwSig->sig.path.drift = followupCwSig.sig.drift;
			cwSig->sig.path.width = 0;
			cwSig->sig.path.power = 0;
			cwSig->sig.pol = POL_UNINIT;
			cwSig->sig.sigClass = signal->getClass();
			cwSig->sig.reason = signal->getReason();
			if (cwSig->sig.sigClass == CLASS_UNINIT)
				cwSig->sig.sigClass = CLASS_RFI;
			if (cwSig->sig.reason == CLASS_REASON_UNINIT)
				cwSig->sig.reason = NO_SIGNAL_FOUND;
			cwSig->sig.subchannelNumber
					= act->getChannel()->getSubchannel(cwSig->sig.path.rfFreq);
			cwSig->sig.signalId.dxNumber = state->getNumber();
			cwSig->sig.signalId.activityId = act->getActivityId();
			cwSig->sig.signalId.activityStartTime = act->getStartTime();
			cwSig->sig.signalId.number = -1;
			cwSig->sig.origSignalId = followupCwSig.sig.origSignalId;
			Debug(DEBUG_CONTROL, cwSig->sig.path.rfFreq, "cw followup rf");
			msg = msgList->alloc(SEND_CANDIDATE_CW_POWER_SIGNAL, activityId,
					cwSig, sizeof(::CwPowerSignal), blk);
			respQ->send(msg);
		}
		else if (pulseFollowupSignal) {
			// for a pulse signal, just send a PulseSignalHeader with
			// no pulses
			blk = partitionSet->alloc(sizeof(PulseSignalHeader));
			Assert(blk);
			PulseSignalHeader *pSig
					= static_cast<PulseSignalHeader *> (blk->getData());
			FollowUpPulseSignal followupPulseSig
					= pulseFollowupSignal->getSignal();
			pSig->sig.path.rfFreq = followupPulseSig.sig.rfFreq;
			pSig->sig.path.drift = followupPulseSig.sig.drift;
			pSig->sig.origSignalId = followupPulseSig.sig.origSignalId;
			pSig->sig.path.width = 0;
			pSig->sig.path.power =  0;
			pSig->sig.pol = POL_UNINIT;
			pSig->sig.sigClass = signal->getClass();
			pSig->sig.reason = signal->getReason();
			if (pSig->sig.sigClass == CLASS_UNINIT)
				pSig->sig.sigClass = CLASS_RFI;
			if (pSig->sig.reason == CLASS_REASON_UNINIT)
				pSig->sig.reason = NO_SIGNAL_FOUND;
			pSig->sig.subchannelNumber
					= act->getChannel()->getSubchannel(pSig->sig.path.rfFreq);
			pSig->sig.signalId.dxNumber = state->getNumber();
			pSig->sig.signalId.activityId = act->getActivityId();
			pSig->sig.signalId.activityStartTime = act->getStartTime();
			pSig->sig.signalId.number = -1;
			pSig->sig.origSignalId = followupPulseSig.sig.origSignalId;
			pSig->cfm.pfa = 0;
			pSig->cfm.snr = 0;
			pSig->train.pulsePeriod = 0;
			pSig->train.numberOfPulses = 0;
			pSig->train.res = RES_UNINIT;
			Debug(DEBUG_CONTROL, pSig->sig.path.rfFreq, "pulse followup rf");
			msg = msgList->alloc(SEND_CANDIDATE_PULSE_SIGNAL, activityId,
					pSig, sizeof(PulseSignalHeader), blk);
			respQ->send(msg);
		}
	}
	act->releaseFollowupSignals();

	msg = msgList->alloc(DONE_SENDING_CANDIDATES, activityId);
	respQ->send(msg);
}

//
// sendSignals: send all the signals found to the SSE
//
// Notes:
//		All detectors send these messages.
//
void
ControlTask::sendSignals(int32_t activityId)
{
	Activity *act;

	if (!(act = state->findActivity(activityId)))
		return;

	DetectionStatistics statistics;
	act->getDetectionStatistics(statistics);
	statistics.totalCandidates = act->getCandidateCount(ANY_TYPE);
	statistics.cwCandidates = act->getCandidateCount(CW_POWER);
	statistics.pulseCandidates = act->getCandidateCount(PULSE);
	statistics.candidatesOverMax = act->getCandidatesOverMax();
	statistics.totalSignals = act->getSignalCount();
	statistics.cwSignals = act->getSignalCount(CW_POWER);
	statistics.pulseSignals = act->getSignalCount(PULSE);
	act->setDetectionStatistics(statistics);

//	cout << statistics << endl;

	// record the total signal count
	int32_t signals = act->getSignalCount();

	// send the signal count
	MemBlk *blk = partitionSet->alloc(sizeof(DetectionStatistics));
	Assert(blk);
	DetectionStatistics *stats = static_cast<DetectionStatistics *> (blk->getData());
	*stats = statistics;
	Msg *msg = msgList->alloc(BEGIN_SENDING_SIGNALS, activityId, stats,
			sizeof(DetectionStatistics), blk);
	respQ->send(msg);

	// send the signals in supercluster order; remove each signal as
	// it is sent
	for (int32_t i = 0; i < signals; i++) {
 		Signal *signal = act->getNthSignal(i);
 		CwSignal *cwSignal = signal->getCw();
 		PulseSignal *pulseSignal = signal->getPulse();
 		if (cwSignal) {
			blk = partitionSet->alloc(sizeof(CwPowerSignal));
			Assert(blk);
			CwPowerSignal *cwSig = static_cast<CwPowerSignal *> (blk->getData());
			*cwSig = cwSignal->getSignal();
#ifdef notdef
			cwSig->sig.subchannelNumber = act->getSubchannel(cwSig->sig.rfFreq);
			cwSig->sig.pol = superClusterer->getNthPolarization(i);
			cwSig->sig.signalId = superClusterer->getNthSignalId(i);
#endif
			msg = msgList->alloc(SEND_CW_POWER_SIGNAL, activityId, cwSig,
					sizeof(CwPowerSignal), blk);
			respQ->send(msg);
		}
		else if (pulseSignal) {
			Debug(DEBUG_NEVER, i, "pd signal");
			PulseSignalHeader *sig = pulseSignal->getSignal();
			Debug(DEBUG_NEVER, (void *) sig, "pulse signal");
			Debug(DEBUG_NEVER, sig->train.numberOfPulses, "pulses");

			size_t len = sizeof(PulseSignalHeader)
					+ sig->train.numberOfPulses * sizeof(::Pulse);
			blk = partitionSet->alloc(len);
			Assert(blk);
			PulseSignalHeader *pSig
					= static_cast<PulseSignalHeader *> (blk->getData());
			*pSig = *sig;
			::Pulse *sp = (::Pulse *) (sig + 1);
			::Pulse *dp = (::Pulse *) (pSig + 1);
			for (int32_t j = 0; j < sig->train.numberOfPulses; ++j)
				*dp++ = *sp++;

			msg = msgList->alloc(SEND_PULSE_SIGNAL, activityId, pSig, len, blk);
			respQ->send(msg);
		}
	}
	act->releaseSignals();

	msg = msgList->alloc(DONE_SENDING_SIGNALS, activityId);
	respQ->send(msg);
}

//
// sendBadBands: send the list of bad bands
//
void
ControlTask::sendBadBands(int32_t activityId)
{
	Activity *act;

	if (!(act = state->findActivity(activityId)))
		return;

	CwBadBandList *cwBadBands = act->getCwBadBandList();
	Assert(cwBadBands);

	MemBlk *blk = partitionSet->alloc(sizeof(Count));
	Assert(blk);
	Count *count = static_cast<Count *> (blk->getData());
	count->count = cwBadBands->getSize();
	PulseBadBandList *pulseBadBands;
	for (int i = 0; i < MAX_RESOLUTIONS; ++i) {
		pulseBadBands = act->getPulseBadBandList((Resolution) i);
		Assert(pulseBadBands);
		count->count += pulseBadBands->getSize();
	}

	Msg *msg = msgList->alloc(BEGIN_SENDING_BAD_BANDS, activityId, count,
			sizeof(Count), blk);
	respQ->send(msg);

	// send all the CW bad bands
	for (int i = 0; i < cwBadBands->getSize(); ++i) {
		blk = partitionSet->alloc(sizeof(::CwBadBand));
		Assert(blk);
		::CwBadBand badBand = cwBadBands->getNth(i);
		::CwBadBand *band = static_cast< ::CwBadBand *> (blk->getData());
		*band = badBand;
//		cout << badBand << endl;
		msg = msgList->alloc(SEND_CW_BAD_BAND, activityId, band,
				sizeof(::CwBadBand), blk);
		respQ->send(msg);
	}
	cwBadBands->clear();
	// report each pulse resolution
	for (int i = 0; i < MAX_RESOLUTIONS; ++i) {
		pulseBadBands = act->getPulseBadBandList((Resolution) i);
		Assert(pulseBadBands);
		for (int j = 0; j < pulseBadBands->getSize(); ++j) {
			blk = partitionSet->alloc(sizeof(::PulseBadBand));
			Assert(blk);
			::PulseBadBand badBand = pulseBadBands->getNth(j);
			::PulseBadBand *band =
					static_cast< ::PulseBadBand *> (blk->getData());
			*band = badBand;
//			cout << badBand << endl;
			msg = msgList->alloc(SEND_PULSE_BAD_BAND, activityId, band,
					sizeof(::PulseBadBand), blk);
			respQ->send(msg);
		}
		pulseBadBands->clear();
	}
	msg = msgList->alloc(DONE_SENDING_BAD_BANDS, activityId);
	respQ->send(msg);
}

////////////////////////////////////////////////////////////////////////
// secondary-specific message handlers
///////////////////////////////////////////////////////////////////////

//
// addCwCandidate: add a CW candidate only if it is really a candidate
//
// Notes:
//		If it is a followup detection, the SSE may send candidates
//		which are not really candidates because they failed followup
//		detection at the primary; in this case, the signal
//		type will not be CLASS_CAND.  In this case, we just
//		ignore the candidate
//
Error
ControlTask::addCwCandidate(Msg *msg, Activity *act)
{
	CwPowerSignal *cwSig = static_cast<CwPowerSignal *> (msg->getData());
	SuperClusterer *superClusterer = act->getSuperClusterer();
	Assert(superClusterer);
	if (cwSig->sig.sigClass == CLASS_CAND) {
		cwSig->sig.signalId = superClusterer->generateNewSignalId();
		act->addCwCandidate(cwSig, SECONDARY);
	}
	Debug(DEBUG_CONTROL, act->getCandidateCount(CW_POWER),
			"cw candidate count");
	return (0);
}

//
// addPulseCandidate: add a pulse signal to the candidate list
//
Error
ControlTask::addPulseCandidate(Msg *msg, Activity *act)
{
	PulseSignalHeader *pSig = static_cast<PulseSignalHeader *> (msg->getData());
	SuperClusterer *superClusterer = act->getSuperClusterer();
	Assert(superClusterer);
	Debug(DEBUG_CONFIRM, pSig->sig.sigClass, "pSig->sig.sigClass");
	if (pSig->sig.sigClass == CLASS_CAND) {
		Debug(DEBUG_CONFIRM, 0, "adding");
		pSig->sig.signalId = superClusterer->generateNewSignalId();
//		cout << *pSig;
		act->addPulseCandidate(pSig, SECONDARY);
		Msg *cMsg = msgList->alloc();

		msg->forward(cMsg);
		confirmationQ->send(cMsg);
	}

	Debug(DEBUG_CONTROL, act->getCandidateCount(PULSE),
			"pulse candidate count");
	return (0);
}

//
// Notes:
//
Error
ControlTask::endCandidates(Msg *msg, Activity *act)
{
	MemBlk *blk = partitionSet->alloc(sizeof(Count));
	Assert(blk);
	Count *count = static_cast<Count *> (blk->getData());
	count->count = act->getCandidateCount(ANY_TYPE);
	Msg *rMsg = msgList->alloc(BEGIN_SENDING_CANDIDATE_RESULTS,
			act->getActivityId(), count, sizeof(Count), blk);
	respQ->send(rMsg);

	return (0);
}

//
// Notes:
//		Process a CW coherent signal from a primary detector.  A message
//		is sent to the confirmation task to start the confirmation of
//		the signal.
//
Error
ControlTask::sendCwCoherentSignal(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	confirmationQ->send(cMsg);

	return (0);
}

//
// endCwCoherentSignals: done with coherent signals
//
// Notes:
//		If we have no signals to confirm, we are done
//
Error
ControlTask::endCwCoherentSignals(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	confirmationQ->send(cMsg);

	return (0);
}

/**
 * Determine whether value is a power of 2.
 */
bool
ControlTask::isPow2(uint32_t value)
{
	while (value > 1) {
		if (value & 1)
			return (false);
		value >>= 1;
	}
	return (true);
}

/**
 * Return the largest power of 2 less than value.
 */
uint32_t
ControlTask::pow2(uint32_t value)
{
	Assert(value);
	uint32_t powerOfTwo;
	for (powerOfTwo = 1; powerOfTwo <= value; powerOfTwo <<= 1)
		;
	powerOfTwo >>= 1;
	return (powerOfTwo);
}

}
