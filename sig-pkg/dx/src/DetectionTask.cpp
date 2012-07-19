/*******************************************************************************

 File:    DetectionTask.cpp
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
// Signal detection control task
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/DetectionTask.cpp,v 1.5 2009/02/24 18:51:49 kes Exp $
//
#include <iostream>
#include <DxOpsBitset.h>
#include "ControlTask.h"
#include "CwTask.h"
#include "DetectionTask.h"
#include "PulseTask.h"
#include "SignalIdGenerator.h"

using std::cout;
using std::endl;

namespace dx {

DetectionTask *DetectionTask::instance = 0;

DetectionTask *
DetectionTask::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new DetectionTask("DetectionTask");
	l.unlock();
	return (instance);
}


DetectionTask::DetectionTask(string name_):
		QTask(name_, DETECTION_PRIO), activity(0), controlQ(0), cwQ(0),
		pulseQ(0), msgList(0), state(0)
{
}

DetectionTask::~DetectionTask()
{
}

void
DetectionTask::extractArgs()
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

	// extract startup args
	DetectionArgs *detectionArgs = static_cast<DetectionArgs *> (args);
	Assert(detectionArgs);
	controlQ = detectionArgs->controlQ;
	Assert(controlQ);
#ifdef notdef
	superClusterer = detectionArgs->superClusterer;
	Assert(superClusterer);
#endif

	msgList = MsgList::getInstance();
	Assert(msgList);
	state = State::getInstance();
	Assert(state);

	// create and start the dependent tasks
	createTasks();
	startTasks();
}

/**
 * Create tasks dependent on detection task.
 */
void
DetectionTask::createTasks()
{
	// create the CW detection task
	cwTask = CwTask::getInstance();
	Assert(cwTask);
	cwQ = cwTask->getInputQueue();
	Assert(cwQ);

	// create the pulse detection task
	pulseTask = PulseTask::getInstance();
	Assert(pulseTask);
	pulseQ = pulseTask->getInputQueue();
	Assert(pulseQ);
}

/**
 * Start the dependent tasks.
 */
void
DetectionTask::startTasks()
{
	// start the CW detection task
	cwArgs = CwArgs(getInputQueue());
	cwTask->start(&cwArgs);

	// start the pulse detection task
	pulseArgs = PulseArgs(getInputQueue());
	pulseTask->start(&pulseArgs);
}

void
DetectionTask::handleMsg(Msg *msg)
{
	DxOpsBitset operations;

	switch (msg->getCode()) {
	case StartDetection:
		// start the signal detection
		startActivity(msg);
		break;
	case CwComplete:
		if (!activity)
			break;
		operations = params.operations;
		if (activity->getState() == DX_ACT_STOPPING)
			sendDetectionComplete();
		else
			startPulseDetection();
		break;
	case PulseComplete:
		sendDetectionComplete();
		break;
	case STOP_DX_ACTIVITY:
		stopActivity(msg);
		break;
	case ActivityStopped:
		sendDetectionComplete();
		break;
	case SHUTDOWN_DX:
	case RESTART_DX:
		stopActivity(msg, true);
		break;
	default:
		break;
	}
}

//
// startActivity: start the next signal detection
//
// Synopsis:
//		void startActivity(msg);
//		Msg *msg;				ptr to message
// Returns:
//		Nothing.
// Description:
//		Starts signal detection.
// Notes:
//		All activity information has been stored in the
//		Activity structure assigned to this activity.
//
void
DetectionTask::startActivity(Msg *msg)
{
	Debug(DEBUG_DETECT, 0, "");

	const SseInterfaceHeader& hdr = msg->getHeader();

	// the activity must exist and be DX_ACT_PEND_SD
	Activity *act;
	if (!(act = state->findActivity(hdr.activityId))) {
		LogError(ERR_NSA, msg->getActivityId(), "activity %d",
				msg->getActivityId());
		return;
	}
	else if (act->getState() != DX_ACT_PEND_SD) {
		LogError(ERR_ANS, act->getActivityId(), "activity %d, state %d",
				act->getActivityId(), act->getState());
		return;
	}

	// record the activity and get the activity parameters
	activity = act;
	params = activity->getActivityParams();

	// clear the hit list and set the activity parameters
	SuperClusterer *superClusterer = activity->getSuperClusterer();
	Assert(superClusterer);
	Assert(!superClusterer->getCount());

	// compute the bottom edge of the frequency range, which is 1/2
	// subchannel below the middle of subchannel 0
	float64_t chanWidth = activity->getChannelWidthMHz();
	float64_t deltaFreq = -chanWidth / 2;
	deltaFreq -= activity->getSubchannelWidthMHz() / 2;

	// initialize the signal ID for this activity
	NssDate startTime = activity->getActualStartTime();
	SignalIdGenerator sigGen(state->getSerialNumber(), params.activityId,
			startTime);
	/////////////////////////////////////////////////////////////////////
	// NOTE: this code will have to be modified when we run multiple
	// pulse resolutions, because there is only one resolution specified
	// to the SuperClusterer.
	/////////////////////////////////////////////////////////////////////
	int32_t spectra = activity->getSpectra(params.daddResolution);
	float64_t binWidth = activity->getBinWidthHz(params.daddResolution);

	superClusterer->setObsParams(params.activityId, spectra,
			params.dxSkyFreq + deltaFreq, chanWidth, binWidth, sigGen);
	superClusterer->setSuperClusterGap(HZ_TO_MHZ(params.clusteringFreqTolerance));

	// send a message to the control task indicating that signal detection
	// has started
	Msg *cMsg = msgList->alloc(SIGNAL_DETECTION_STARTED, hdr.activityId);
	controlQ->send(cMsg);

	Debug(DEBUG_DETECT, hdr.activityId, "starting CWD");

	// do CW detection first, if it's enabled, otherwise start
	// pulse detection
	startCwDetection();
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
DetectionTask::stopActivity(Msg *msg, bool stopAll)
{

	// if no activity is detecting, ignore the message
	if (!activity ||
			(!stopAll && activity->getActivityId() != msg->getActivityId())) {
		Msg *cMsg = msgList->alloc((DxMessageCode) ActivityStopped,
				msg->getActivityId());
		cMsg->setUnit((sonata_lib::Unit) UnitDetect);
		controlQ->send(cMsg);
		return;
	}

	// stop both of the detectors (only one can be running,
	// but we don't know which one)
	stopDetection();
}

//
// sendDetectionComplete: send a detection complete message
//
// Notes:
//		The actual message sent depends upon the current state
//		of the activity.  If its state is DX_ACT_RUN_SD, then
//		a message is sent to the control task indicating that
//		basic signal detection is done.
void
DetectionTask::sendDetectionComplete()
{
	if (!activity)
		return;

	Debug(DEBUG_DETECT, activity->getActivityId(), "activityId");
	Debug(DEBUG_DETECT, (int32_t) activity->getState(), "state");
	Msg *cMsg;
	if (activity->getState() == DX_ACT_STOPPING) {
		cMsg = msgList->alloc((DxMessageCode) ActivityStopped,
				activity->getActivityId());
		cMsg->setUnit((sonata_lib::Unit) UnitDetect);
	}
	else {
		cMsg = msgList->alloc((DxMessageCode) DetectionComplete,
				activity->getActivityId());
	}
	controlQ->send(cMsg);

	activity = 0;
}

void
DetectionTask::startCwDetection()
{
	Debug(DEBUG_DETECT, activity->getActivityId(), "sending CWD start msg");
	activity->setState(DX_ACT_RUN_SD);
	Msg *cwMsg = msgList->alloc((DxMessageCode) StartDetection,
			activity->getActivityId());
	Debug(DEBUG_DETECT, (void *) cwQ, "sending CWD start msg");
	cwQ->send(cwMsg);
	Debug(DEBUG_DETECT, activity->getActivityId(), "CWD start msg sent");
}

void
DetectionTask::startPulseDetection()
{
	activity->setState(DX_ACT_RUN_SD);
	Msg *pdMsg = msgList->alloc((DxMessageCode) StartDetection,
			activity->getActivityId());
	pulseQ->send(pdMsg);
}

void
DetectionTask::stopDetection()
{
	Debug(DEBUG_DETECT, activity->getActivityId(), "act");
	activity->setState(DX_ACT_STOPPING);

	Msg *cwMsg = msgList->alloc(STOP_DX_ACTIVITY, activity->getActivityId());
	cwQ->send(cwMsg);

	Msg *pMsg = msgList->alloc(STOP_DX_ACTIVITY, activity->getActivityId());
	pulseQ->send(pMsg);
}

}
