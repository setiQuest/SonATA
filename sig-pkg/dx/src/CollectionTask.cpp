/*******************************************************************************

 File:    CollectionTask.cpp
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

// Data collection task
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/CollectionTask.cpp,v 1.7 2009/03/06 21:58:56 kes Exp $
//
#include <iostream>
#include <sys/time.h>
#include <sys/socket.h>
#include <sseDxInterface.h>
#include <DxOpsBitset.h>
#include "Alarm.h"
#include "CollectionTask.h"
//#include "ControlTask.h"
#include "DxErr.h"
#include "Spectrometer.h"
#include "SubchannelMask.h"

using std::cout;
using std::endl;

namespace dx {

/**
 * Collection task.
 *
 * Description:\n
 * 	This task controls the data collection process; it handles all messages
 * 	from the control task and sets up the collection components (reader and
 * 	worker tasks and spectrometer).  The DX had input tasks for each input
 * 	data type (baselines, confirmation data, cw data and pulse data); those
 * 	functions are now performed by the spectrometer, which deposits each
 * 	data type in its respective buffer.  As a result, this task is much
 * 	simpler than it used to be.
 */
CollectionTask *CollectionTask::instance = 0;

CollectionTask *
CollectionTask::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new CollectionTask("CollectionTask");
	l.unlock();
	return (instance);
}

CollectionTask::CollectionTask(string name_):
		QTask(name_, COLLECTION_PRIO), started(false), activityId(-1),
		activity(0), channel(0), receiver(0), worker(0), controlQ(0),
		cmdArgs(0), msgList(0), partitionSet(0), state(0)
{
}

CollectionTask::~CollectionTask()
{
}

void
CollectionTask::extractArgs()
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

	// extract arguments
	CollectionArgs *collectionArgs = static_cast<CollectionArgs *> (args);
	Assert(collectionArgs);
	controlQ = collectionArgs->controlQ;

	cmdArgs = Args::getInstance();
	Assert(cmdArgs);
	msgList = MsgList::getInstance();
	Assert(msgList);
	partitionSet = PartitionSet::getInstance();
	Assert(partitionSet);
	state = State::getInstance();
	Assert(state);

	// create and start the input task
	inputArgs = InputArgs(UnitInput);
	input = new InputTask("Input");
	Assert(input);
	input->start(&inputArgs);
	inQ = input->getInputQueue();

	// create and start the worker task
	workerArgs = WorkerArgs(UnitWorker, getInputQueue());
	worker = new WorkerTask("Worker");
	Assert(worker);
	worker->start(&workerArgs);
}

void
CollectionTask::handleMsg(Msg *msg)
{
	switch (msg->getCode()) {
	case Tune:
		tune(msg);
		break;
	case START_TIME:
		startActivity(msg);
		break;
	case BASELINE_INIT_ACCUM_STARTED:
		setBaselineStarted(msg);
		break;
	case BASELINE_INIT_ACCUM_COMPLETE:
		setBaselineComplete(msg);
		break;
	case DATA_COLLECTION_STARTED:
		setCollectionStarted(msg);
		break;
	case DATA_COLLECTION_COMPLETE:
		setCollectionStopped(msg);
		break;
	case STOP_DX_ACTIVITY:
		stopActivity(msg);
		break;
	case SHUTDOWN_DX:
	case RESTART_DX:
		stopActivity(msg, true);
		break;
	default:
		Fatal(ERR_IMT);
		break;
	}
}

/**
 * Tune the DX.
 *
 * Description:\n
 * 	The SSE sends a tune message requesting a specific center frequency,
 * 	which must be the center frequency of the channel specified.
 */
//
// tune: request the DX tuning
//
// Notes:
//		This is now a dummy call, since there are no dsp boards
//		to consult about the tuning.  Instead, just send a message
//		confirming the frequency.
//
void
CollectionTask::tune(Msg *msg)
{
	const SseInterfaceHeader& hdr = msg->getHeader();

	// the activity must exist
	Activity *act;
	if (!(act = state->findActivity(hdr.activityId))) {
		LogError(ERR_NSA, msg->getActivityId(), "activity %d",
				msg->getActivityId());
		return;
	}
	activityId = hdr.activityId;
	activity = act;
	activity->setState(DX_ACT_TUNED);

	MemBlk *blk = partitionSet->alloc(sizeof(DxTuned));
   	Assert(blk);
   	DxTuned *tuneInfo = static_cast<DxTuned *> (blk->getData());
   	tuneInfo->dxSkyFreq = act->getSkyFreq();
   	tuneInfo->dataCollectionLength = act->getDataCollectionTime();
   	tuneInfo->dataCollectionFrames = act->getFrames();
	Msg *cMsg = msgList->alloc(DX_TUNED, act->getActivityId(), tuneInfo,
			sizeof(DxTuned), blk);
	controlQ->send(cMsg);
}

/**
* Start the activity.
*
* Description:\n
*	Sets the starting time in all channels, which will start data
*	collection as soon as a packet is received with a timestamp equal
*	to or later than the specified starting time.\n\n
* Notes:\n
*	The activity state must be DX_ACT_TUNED.
*/
void
CollectionTask::startActivity(Msg *msg)
{
	const SseInterfaceHeader& hdr = msg->getHeader();

	// the activity must exist and be DX_ACT_TUNED.
	Activity *act;
	if (activityId != hdr.activityId) {
		LogError(ERR_SWA, msg->getActivityId(), "activity %d",
				msg->getActivityId());
		return;
	}
	else if (!(act = state->findActivity(hdr.activityId))) {
		LogError(ERR_NSA, msg->getActivityId(), "activity %d",
				msg->getActivityId());
		return;
	}
	else if (act->getState() != DX_ACT_TUNED) {
		LogError(ERR_ANS, act->getActivityId(), "activity %d, state %d",
				act->getActivityId(), act->getState());
		return;
	}

	// get the starting time
	const StartActivity *startTime = static_cast<StartActivity *>
			(msg->getData());
	act->setStartTime(startTime->startTime);

	// set the subchannel mask
	act->createSubchannelMask();
	if (act->allSubchannelsMasked()) {
		LogWarning(ERR_ASM, act->getActivityId(), "activity %d",
				act->getActivityId());
	}

#ifdef notdef
	// if this is a ZX, create the requested subchannel mask
	act->createRequestedSubchannelMask();
#endif

	// create the reader and initialize the worker tasks
	createReceiver();

	const DxActivityParameters& params = activity->getActivityParams();
	DxOpsBitset operations(params.operations);

	started = false;
	channel = activity->getChannel();
	if (operations.test(BASELINING) && params.baselineInitAccumHalfFrames > 0)
		activity->setState(DX_ACT_PEND_BASE_ACCUM);
	else
		activity->setState(DX_ACT_PEND_DC);

	// set the stop mask
	stopMask.reset();
	stopMask.set(WORKER_INPUT_BIT);

	// set up the observation parameters for the spectrometer
	setStartObs();
	startWorker(activity);

	// start processing packets
	startReceiver();
}

//
// stopActivity: stop an activity
//
// Synopsis:
//		void stopActivity(msg, stopAll);
//		Msg *msg;				ptr to message
//		bool stopAll;				stop all activities
// Returns:
//		Nothing.
// Description:
//		Stops either the specified activity or any activity
//		which is currently active.
// Notes:
//		Sends messages to the collection tasks.
//		Note that this task never kills a collecting
//		activity directly; instead, it sends
//		messages to the individual collection subtasks
//		instructing them to kill the task and report back
//		that it has been completed.
//
void
CollectionTask::stopActivity(Msg *msg, bool stopAll)
{
	// if no activity is in collection state, just issue a stopped message
	Debug(DEBUG_COLLECT, msg->getActivityId(), "msg act");
	Debug(DEBUG_COLLECT, (void *) activity, "actp");
	if (!activity) {
		sendActivityStopped(msg);
		return;
	}

	if (!stopAll) {
		const SseInterfaceHeader& hdr = msg->getHeader();

		// if the activity ID doesn't match, ignore the message
		if (activityId != hdr.activityId)
			return;
	}

	stopReceiver();
	stopWorker(activity);
	// we're either stopping any collecting activity or the one
	// which is currently collecting
	if (activity->getState() != DX_ACT_STOPPING)
		activity->setState(DX_ACT_STOPPING);

	sendActivityStopped(msg);
}

/**
 * Log the baselining as started.
 *
 * Description:\n
 * 	Sends message to the control task notifying it that baselining has
 * 	begun.
 */
void
CollectionTask::setBaselineStarted(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

/**
 * Log the baselining as complete
 */
void
CollectionTask::setBaselineComplete(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

/**
 * setCollectionStarted: notify the control task that data collection
 * 		has started.
 */
void
CollectionTask::setCollectionStarted(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

/**
 * setCollectionStopped: log that an input task has stopped the activity.
 */
void
CollectionTask::setCollectionStopped(Msg *msg)
{
	Debug(DEBUG_COLLECT, msg->getUnit(), "unit");
	Debug(DEBUG_COLLECT, msg->getActivityId(), "act id");
	// figure out which task is responding
	int32_t bit;
	switch (static_cast<dx::Unit> (msg->getUnit())) {
	case UnitWorker:
		bit = WORKER_INPUT_BIT;
		break;
//	case UnitReceiver:
//		bit = RECEIVER_INPUT_BIT;
//		break;
	default:
		LogFatal(ERR_IU, msg->getActivityId(), "unit %d", msg->getUnit());
		break;
	}
	Debug(DEBUG_COLLECT, (int32_t) stopMask.to_ulong(), "stop mask");
	// if all units have stopped the activity, send a status message
	// to the control task indicating that we have stopped the
	// activity
	if (stopMask.any()) {
		stopMask.reset(bit);
//		cout << "stop mask none " << stopMask.none() << endl;
		if (stopMask.none()) {
			stopReceiver();
			Msg *cMsg = msgList->alloc(DATA_COLLECTION_COMPLETE);
			if (activity->getState() == DX_ACT_STOPPING)
				cMsg->setCode(ActivityStopped);
//			else if (activity->getState() == DX_ACT_RUN_DC)
//				activity->setState(DX_ACT_DC_COMPLETE);
			else if (activity->getState() != DX_ACT_DC_COMPLETE)
				activity->setState(DX_ACT_ERROR);
//			cout << "state " << activity->getState() << endl;
			cMsg->setActivityId(activity->getActivityId());
			cMsg->setUnit((sonata_lib::Unit) UnitCollect);
			controlQ->send(cMsg);
			// there's no activity now
			activity = 0;
		}
	}
	string n;
	name(n);
	LogInfo(0, -1, " %s received msg for unit %d. Modified stop mask: %08x",
			n.c_str(), msg->getUnit(), stopMask.to_ulong());
	Debug(DEBUG_COLLECT, msg->getUnit(), "unit done")
}

//
// send activity stopped for an activity that is not currently in
//		collection
//
void
CollectionTask::sendActivityStopped(Msg *msg)
{
	Msg *cMsg = msgList->alloc((DxMessageCode) ActivityStopped,
			msg->getActivityId());
	cMsg->setUnit((sonata_lib::Unit) UnitCollect);
	controlQ->send(cMsg);
}

//////////////////////////////////////////////////////////////////////
// methods to start data collection
//////////////////////////////////////////////////////////////////////

void
CollectionTask::setStartObs()
{
	const DxActivityParameters& params = activity->getActivityParams();

	DxOpsBitset operations(params.operations);

	ObsData obs;
	memset(&obs, 0, sizeof(obs));
	obs.cwResolution = params.daddResolution;
	obs.observationId = params.activityId;
	obs.baselineDelay = params.baselineInitAccumHalfFrames;
	obs.baselineWeighting = params.baselineDecay;
	obs.pulseThreshold = params.pd[0].pulseThreshold;
	obs.centerFreq = MHZ_TO_HZ(activity->getSkyFreq()
			- activity->getIfcSkyFreq());
	obs.maxPulsesPerHalfFrame = params.maxPulsesPerHalfFrame;
	obs.maxPulsesPerSubchannelPerHalfFrame =
			params.maxPulsesPerSubchannelPerHalfFrame;
	if (obs.maxPulsesPerSubchannelPerHalfFrame > MAX_SUBCHANNEL_PULSES) {
		LogWarning(ERR_PLE, activity->getActivityId(),
				"max pulses per subchannel per half frame = %d, max = %d",
				obs.maxPulsesPerSubchannelPerHalfFrame,
				MAX_SUBCHANNEL_PULSES);
	}
	obs.ops.reset();
	if (cmdArgs->zeroDCBins())
		obs.ops.set(ZERO_DC_BIN);
	if (params.baselineInitAccumHalfFrames > 0)
		obs.resetBaseline = 1;
	else
		obs.resetBaseline = 0;
	if (cmdArgs->useTaggedData())
		obs.cdOutputOption = tagged_data;
	else
		obs.cdOutputOption = normal;

	if (!operations.test(POWER_CWD))
		obs.cwOutputOption = by_pass;
	else if (cmdArgs->useTaggedData())
		obs.cwOutputOption = tagged_data;
	else
		obs.cwOutputOption = normal;

	if (operations.test(BASELINING))
			obs.blOutputOption = normal;
		else
			obs.blOutputOption = by_pass;

	if (operations.test(PULSE_DETECTION))
		obs.pulseOutputOption = normal;
	else
		obs.pulseOutputOption = by_pass;

	Debug(DEBUG_COLLECT, activity->getActivityId(), "actId");
//	setSubchannelMask(obs, activity);

#ifdef notdef
	// display the subchannel mask
	displaySubchannelMask(obsData);
#endif

	// save the start observation parameters
	activity->setObsData(obs);
}

/**
* Create Ethernet receiver task.
*
* Description:\n
*	A receiver task is created for each activity; it accepts incoming packets
* 	for the channel during data collection, then is destroyed as soon as data
* 	collection is complete.  A Udp object is created to actually receive
* 	the data, and is passed to the receiver task when it is started.\n
*
*/
void
CollectionTask::createReceiver()
{
	receiver = new ReceiverTask("receiver");
}

void
CollectionTask::startReceiver()
{
	receiverArgs = ReceiverArgs(UnitReceiver, activity, cmdArgs->getMcAddr(),
			cmdArgs->getMcPort(), cmdArgs->getMcAddr(), cmdArgs->getMcPort(),
			inQ);
	receiver->start(&receiverArgs);
}

void
CollectionTask::stopReceiver()
{
	receiver->kill();
}

void
CollectionTask::startWorker(Activity *act)
{
	worker->startActivity(act);
}

/**
* Stop the Worker tasks.
*
* Description:\n
*	Sends a DATA_COLLECTION_COMPLETE message to all the EtherWorker tasks
*	then waits for each to terminate before deleting it.\n\n
* Notes:\n
*	All EtherWorkers read from the same queue, so it is necessary to send
*	N stop messages, where N is the number of workers.
* 	THIS

*/
void
CollectionTask::stopWorker(Activity *act)
{
	worker->stopActivity(act);
}

}
