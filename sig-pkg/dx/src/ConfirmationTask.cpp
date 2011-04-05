/*******************************************************************************

 File:    ConfirmationTask.cpp
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
// Candidate signal confirmation control task
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/ConfirmationTask.cpp,v 1.4 2009/05/24 22:44:23 kes Exp $
//
#include "ArchiveTask.h"
#include "CwSignal.h"
#include "ConfirmationTask.h"
//#include "ControlTask.h"
//#include "CwConfirmationTask.h"
#include "Log.h"
//#include "PulseConfirmationTask.h"
//#include "SseOutputTask.h"

namespace dx {

ConfirmationTask *ConfirmationTask::instance = 0;

ConfirmationTask *
ConfirmationTask::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new ConfirmationTask("ConfirmationTask");
	l.unlock();
	return (instance);
}

ConfirmationTask::ConfirmationTask(string tname_):
		QTask(tname_, CONFIRMATION_PRIO), cwCount(0), pulseCount(0),
		activity(0), archiveQ(0), controlQ(0), respQ(0), cwQ(0), pulseQ(0),
		cSem(0), msgList(0), partitionSet(0), state(0)
{
}

ConfirmationTask::~ConfirmationTask()
{
}

void
ConfirmationTask::extractArgs()
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
	ConfirmationArgs *confirmationArgs =
			static_cast<ConfirmationArgs *> (args);
	Assert(confirmationArgs);
	controlQ = confirmationArgs->controlQ;
	Assert(controlQ);
	respQ = confirmationArgs->respQ;
	Assert(respQ);
	archiveQ = confirmationArgs->archiveQ;
	Assert(archiveQ);

	msgList = MsgList::getInstance();
	Assert(msgList);
	partitionSet = PartitionSet::getInstance();
	Assert(partitionSet);
	state = State::getInstance();
	Assert(state);

	// create and start the dependent tasks
	createTasks();
	startTasks();
}

/**
 * Create dependent confirmation tasks.
 */
void
ConfirmationTask::createTasks()
{
	// create the CW confirmation task
	cwTask = CwConfirmationTask::getInstance();
	Assert(cwTask);
	cwQ = cwTask->getInputQueue();
	Assert(cwQ);

	// create the pulse confirmation task
	pulseTask = PulseConfirmationTask::getInstance();
	pulseQ = pulseTask->getInputQueue();
	Assert(pulseQ);
}

/**
 * Start the dependent confirmation tasks.
 */
void
ConfirmationTask::startTasks()
{
	// start the CW confirmation task
	cwConfirmationArgs = CwConfirmationArgs(getInputQueue(), respQ);
	cwTask->start(&cwConfirmationArgs);

	// start the pulse confirmation task
	pulseConfirmationArgs = PulseConfirmationArgs(getInputQueue(), respQ);
	pulseTask->start(&pulseConfirmationArgs);
}

//
// Notes:
//		Confirmation at a secondary detector is driven by the
//		results of the primary detector.
//
void
ConfirmationTask::handleMsg(Msg *msg)
{
	Activity *act;

	if (!(act = state->findActivity(msg->getActivityId()))) {
		//Debug(DEBUG_CONFIRM, (int32_t) msg->getActivityId(), "no activity");
		return;
	}
	switch (msg->getCode()) {
	case StartConfirmation:
		// start the signal detection
		startActivity(msg);
		break;
	case CwConfirmationComplete:
	case PulseConfirmationComplete:
		//Debug(DEBUG_CONFIRM, (int32_t) msg->getCode(), "conf complete");
		if (activity && activity->getMode() == PRIMARY) {
		    if ( activity->getState() == DX_ACT_RUN_SD) {
			//Debug(DEBUG_CONFIRM, activity->getCandidateCount(AnyType),
			//		"candidateCount");
			archiveCandidate(msg);
			// get the next signal to confirm
			Signal *candidate = activity->getFirstCandidate(DETECTED);
			Debug(DEBUG_CONFIRM, (void *) candidate, "first candidate");
			if (candidate)
				confirmCandidate(candidate);
			else {
				if (cwCount)
					sendCwComplete();
				if (pulseCount)
					sendPulseComplete();
				sendConfirmationComplete(activity);
			}
			}
		}
		break;
	case STOP_DX_ACTIVITY:
		//Debug(DEBUG_CONFIRM, (int32_t) msg->getCode(), "stop");
		stopActivity(msg);
		break;
	case SHUTDOWN_DX:
	case RESTART_DX:
		stopActivity(msg, true);
		break;
	case ActivityStopped:
		//Debug(DEBUG_CONFIRM, (int32_t) msg->getCode(), "stopped");
		testActivityStopped(msg);
		break;
	default:
		// if we're processing secondary candidates, we have to handle
		// additional messages
		Debug(DEBUG_CONFIRM, (int32_t) msg->getCode(), "try secondary");
		if (handleSecondaryMsg(msg)) {
			FatalStr((int32_t) msg->getCode(), "msg code");
			LogFatal(ERR_IMT, msg->getActivityId(), "code %d", msg->getCode());
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
ConfirmationTask::handleSecondaryMsg(Msg *msg)
{
	Activity *act;

	Debug(DEBUG_CONFIRM, (int32_t) msg->getCode(), "code");

	// find the activity
	if (!(act = state->findActivity(msg->getActivityId()))) {
		Debug(DEBUG_CONFIRM, (int32_t) msg->getActivityId(), "no activity");
		LogError(ERR_NSA, msg->getActivityId(), "message = %d",
				msg->getCode());
		return (ERR_NSA);
	}

	// make sure we are processing secondary candidates
	if (!act->allowSecondaryMsg()) {
		Debug(DEBUG_CONFIRM, (int32_t) act->allowSecondaryMsg(), "no secondary");
		return (ERR_IMT);
	}

	Debug(DEBUG_CONFIRM, (int32_t) msg->getCode(), "code");
	Error err = 0;
	switch (msg->getCode()) {
	case SEND_CANDIDATE_PULSE_SIGNAL:
	case SEND_CW_COHERENT_SIGNAL:
		err = confirmSecondarySignal(msg, act);
		break;
	case SEND_CW_COHERENT_CANDIDATE_RESULT:
		Debug(DEBUG_CONFIRM, 0, "SEND_CW_COHERENT_CANDIDATE_RESULT");
		err = sendCwCoherentResult(msg, act);
		break;
	case SEND_PULSE_CANDIDATE_RESULT:
		Debug(DEBUG_CONFIRM, 0, "SEND_PULSE_CANDIDATE_RESULT");
		err = sendPulseResult(msg, act);
		break;
	case DONE_SENDING_CW_COHERENT_SIGNALS:
		Debug(DEBUG_CONFIRM, 0, "DONE_SENDING_CW_COHERENT_SIGNALS");
		err = endCwCoherentSignals(msg, act);
		break;
	default:
		err = ERR_IMT;
	}
	return (err);
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
//		For the secondary detector, confirmation is driven by
//		the primary.  When this messages is received, it sends
//		a message to the SSE indicating that results are to
//		follow, then begins waiting for results from the primary specifying
//		the information about the signal which will be used
//		to confirm it.
//
void
ConfirmationTask::startActivity(Msg *msg)
{
	Activity *act;

	//Debug(DEBUG_CONFIRM, (int32_t) activity, "CONFIRMATION");

	if (activity) {
		if (activity->getActivityId() != msg->getActivityId()){
		LogError(ERR_AICF, msg->getActivityId(),
				"activity %d, can't start activity %d",
				activity->getActivityId(), msg->getActivityId());
		return;
		}
	}

	// the activity must exist and be DX_ACT_RUN_SD
	//Debug(DEBUG_CONFIRM, msg->getActivityId(), "find activity");
	if (!(act = state->findActivity(msg->getActivityId()))) {
		Debug(DEBUG_CONFIRM, msg->getActivityId(), "act not found");
		LogError(ERR_NSA, msg->getActivityId(), "activity %d",
				msg->getActivityId());
		return;
	}
	else if (act->getState() != DX_ACT_RUN_SD) {
		Debug(DEBUG_CONFIRM, (int32_t) act->getState(), "bad state");
		LogError(ERR_ANS, act->getActivityId(), "activity %d, state %d",
				act->getActivityId(), act->getState());
		return;
	}

	// record the activity
	activity = act;
	Debug(DEBUG_CONFIRM, (void *) activity, "record act");

	// if there are secondary candidates still in the list,
	// delete them.
	if (secondaryList.size()) {
		SecondaryList::iterator p;
		for (p = secondaryList.begin(); p != secondaryList.end(); ++p) {
			SecondaryList::iterator q = p;
			Assert(msgList->free(*q));
			secondaryList.erase(q);
		}
	}
	// create the confirmation semaphore, destroying any old one
	// that exists.
	if (cSem) {
		delete cSem;
		cSem = 0;
	}
	cSem = new Semaphore("confirmation", 1);
	Assert(cSem);

	// set all confirmation detector bits
	stopMask.set();

	// count the number of candidates
	int32_t candidateCount = activity->getCandidateCount(ANY_TYPE);
	cwCount = activity->getCandidateCount(CW_POWER);
	pulseCount = activity->getCandidateCount(PULSE);

	Debug(DEBUG_CONFIRM, candidateCount, "candidateCount");
	Debug(DEBUG_CONFIRM, cwCount, "cwCount");
	Debug(DEBUG_CONFIRM, pulseCount, "pulseCount");

	// start confirmation
	startConfirmation();

	// for a secondary detection, there's  nothing else to do except wait for
	// signal descriptions to be received
	if (activity->getMode() == SECONDARY) {
		if (!cwCount) {
			stopCwConfirmation();
			sendCwComplete();
		}
		if (!pulseCount) {
			stopPulseConfirmation();
			sendPulseComplete();
		}
		// if there are no candidates, we're done
		if (!candidateCount)
			sendConfirmationComplete(activity);
		return;
	}

	////////////////////////////////////////////
	// primary detector only
	////////////////////////////////////////////

	Debug(DEBUG_CONFIRM, 0, "primary confirm");

	// send a message to the SSE
	// with the count of candidates to be confirmed
	MemBlk *blk = partitionSet->alloc(sizeof(Count));
	Assert(blk);
	Count *count = static_cast<Count *> (blk->getData());
	count->count = cwCount;
	Msg *rMsg = msgList->alloc(BEGIN_SENDING_CW_COHERENT_SIGNALS,
			activity->getActivityId(), count, sizeof(Count), blk);
	respQ->send(rMsg);

	LogInfo(ERR_NE, activity->getActivityId(),
			"start activity, cwCount = %d, pulseCount = %d", cwCount,
			pulseCount);
	// if there are no CWD candidates to confirm, just send
	// a message indicating we are done
	if (!cwCount) {
		stopCwConfirmation();
		sendCwComplete();
	}
	// if there are no PD candidates to confirm, just send
	// a message indicating we are done
	if (!pulseCount) {
		stopPulseConfirmation();
		sendPulseComplete();
	}
	// if there are no candidates, we're done
	if (!candidateCount) {
		sendConfirmationComplete(activity);
		return;
	}
	// we have at least one candidate; start confirmation
	Debug(DEBUG_CONFIRM, 1, "confirm first candidate");
	confirmCandidate(activity->getFirstCandidate());
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
//		Sends messages to the detection tasks.
//		Note that this task never kills a detecting
//		activity directly; instead, it sends
//		messages to the individual detection subtasks
//		instructing them to kill the task and report back
//		that it has been completed.
//
void
ConfirmationTask::stopActivity(Msg *msg, bool stopAll)
{
	int32_t activityId = msg->getActivityId();

	// if no activity is detecting, or wrong activity is
	// detecting, ignore the messag
	if (!activity || (!stopAll && activity->getActivityId() != activityId)) {
		sendActivityStopped(msg);
		return;
	}

	// stop both of the detectors
	stopConfirmation();
}

//
// testActivityStopped: test whether all active confirmation
//		detectors have responded to a stop request, then send
//		an activity stopped message if they have
// Notes:
//		At least one detector bit must be set by startActivity
//		in order for this to work
//
void
ConfirmationTask::testActivityStopped(Msg *msg)
{
	int32_t bit = 0;
	switch (static_cast<dx::Unit> (msg->getUnit())) {
	case UnitCwConfirmation:
		bit = CWD_CONFIRM_BIT;
		break;
	case UnitPulseConfirmation:
		bit = PD_CONFIRM_BIT;
		break;
	default:
		break;
	}
	Debug(DEBUG_CONFIRM, (int32_t) stopMask.count(),  "stopMask");
	if (stopMask.any()) {
		stopMask.reset(bit);
	    Debug(DEBUG_CONFIRM,(int32_t)  stopMask.count(),  "stopMask");
		if (stopMask.none()) {
			sendActivityStopped(msg);
			activity = 0;
		}
	}
}

//
// sendActivityStopped: acknowledge that the activity has been stopped
//
void
ConfirmationTask::sendActivityStopped(Msg *msg)
{
	Debug(DEBUG_CONFIRM, msg->getActivityId(), "actId");

#ifdef notdef
	// remove the rest of the candidates
	activity->releaseCandidates(DETECTED);

	// stop the process
	sendCwComplete();
#endif

	// notify the control task
	Msg *cMsg = msgList->alloc((DxMessageCode) ActivityStopped,
			msg->getActivityId());
	cMsg->setUnit((sonata_lib::Unit) UnitConfirm);
	controlQ->send(cMsg);
}

//
// archiveCandidate: notify the archive task that a candidate is
//		ready for archiving
//
// Notes:
//		The candidate state must be set to Confirmed before
//		the archive task is notified.
//
void
ConfirmationTask::archiveCandidate(Msg *msg)
{
	if (!activity)
		return;
//	Debug(DEBUG_ALWAYS, 0, "");
	Signal *signal = static_cast<Signal *> (msg->getData());
//	Debug(DEBUG_ALWAYS, (int32_t) signal, "signal");
	SignalId signalId = signal->getSignalId();

	Debug(DEBUG_ARCHIVE,signalId.number, "CWDConfComplete id = ");
//	cout << "act " << activity->getActivityId()
//	     << " sig id " << signalId.number;
//	cout << "state " << signal->getState() << endl;

	Signal *candidate;
	if (!(candidate = activity->findCandidate(signalId)))
		return;

	Msg *aMsg = msgList->alloc();
	msg->forward(aMsg);
	archiveQ->send(aMsg);
}

//
// sendCwComplete: send a Cw confirmation complete message
//		to the SSE
//
void
ConfirmationTask::sendCwComplete()
{

	// This code has been moved to sendConfirmationComplete
	// since it is the last message the sse sees during primary
	// confirmation. Thus, we don't want to send it until pulse
	// detection completes as well.

}

//
// sendPulseComplete: send a PD confirmation complete message
//		to the SSE
//
// Notes:
//		This method is just a placeholder; it is not necessary
//		to notify the SSE.
//
void
ConfirmationTask::sendPulseComplete()
{
}

//
// sendConfirmationComplete: send a detection complete message
//
// Notes:
//		The actual message sent depends upon the current state
//		of the activity.  If its state is DX_ACT_RUN_DC, then
//		a message is sent to the control task indicating that
//		basic signal detection is done.
//
void
ConfirmationTask::sendConfirmationComplete(Activity *act)
{
	Debug(DEBUG_CONFIRM, (void *) act, "act");
	if (!act)
		return;

	if (act->getMode() == PRIMARY) {
		Msg *rMsg = msgList->alloc(DONE_SENDING_CW_COHERENT_SIGNALS,
				act->getActivityId());
		respQ->send(rMsg);
	}

	// zero this for primary-only mode and secondary mode

	if (!act->allowSecondaryMsg() || act->getMode() == SECONDARY) {
		activity = 0;
	}

	Msg *cMsg = msgList->alloc((DxMessageCode) ConfirmationComplete,
			act->getActivityId());
	controlQ->send(cMsg);

}

//
// startConfirmation: start confirmation
//
void
ConfirmationTask::startConfirmation()
{
	startCwConfirmation();
	startPulseConfirmation();
}

//
// startCwConfirmation: start this activity for confirmation
//
// Notes:
//
void
ConfirmationTask::startCwConfirmation()
{
	Msg *cwMsg = msgList->alloc((DxMessageCode) StartConfirmation,
			activity->getActivityId());
	cwQ->send(cwMsg);
}

//
// startPulseConfirmation: start this activity for confirmation
//
// Notes:
//
void
ConfirmationTask::startPulseConfirmation()
{
	Msg *pMsg = msgList->alloc((DxMessageCode) StartConfirmation,
			activity->getActivityId());
	pulseQ->send(pMsg);
}

void
ConfirmationTask::stopConfirmation()
{
	Debug(DEBUG_CONFIRM, activity->getActivityId(), "act");
	activity->setState(DX_ACT_STOPPING);
	stopCwConfirmation();
	stopPulseConfirmation();
}

void
ConfirmationTask::stopCwConfirmation()
{
	Debug(DEBUG_CONFIRM, activity->getActivityId(), "stopping CWD");
	Msg *cwMsg = msgList->alloc(STOP_DX_ACTIVITY, activity->getActivityId());
	cwQ->send(cwMsg);
	Debug(DEBUG_CONFIRM, activity->getActivityId(), "sent stop");
}

void
ConfirmationTask::stopPulseConfirmation()
{
	Debug(DEBUG_CONFIRM, activity->getActivityId(), "stopping PD");
	Msg *pulseMsg = msgList->alloc(STOP_DX_ACTIVITY, activity->getActivityId());
	pulseQ->send(pulseMsg);
}

//
// confirmCandidate: confirm the next candidate in the list
//
void
ConfirmationTask::confirmCandidate(Signal *candidate)
{
	if (candidate->getCw())
		confirmCw(candidate);
	else
		confirmPulse(candidate);
}

//
// confirmCw: perform confirmation on a CW candidate
//
// Notes:
//		Send a pointer to the candidate signal to the
//		CWD confirmation task.
//
void
ConfirmationTask::confirmCw(Signal *cwSig)
{
	Assert(cwSig);

	LogInfo(ERR_NE, activity->getActivityId(), "cwSig rf %lf",
			cwSig->getRfFreq());

	// send the signal to the confirmation task
	Msg *cMsg = msgList->alloc((DxMessageCode) ConfirmCandidate,
			activity->getActivityId(), cwSig, sizeof(Signal *), 0,
			STATIC);
	cwQ->send(cMsg);
}

//
// confirmPulse: confirm a pulse signal
//
// Notes:
//		A pulse signal does not require confirmation at the primary, but
//		the confirmation data containing the signal must be retrieved
//		and attached to the candidate for archiving.
//
void
ConfirmationTask::confirmPulse(Signal *pulseSig)
{
	Assert(pulseSig);

	LogInfo(ERR_NE, activity->getActivityId(), "pulseSig rf %lf",
			pulseSig->getRfFreq());

	// send the signal to the confirmation task
	Msg *pMsg = msgList->alloc((DxMessageCode) ConfirmCandidate,
			activity->getActivityId(), pulseSig, sizeof(Signal *), 0,
			STATIC);
	pulseQ->send(pMsg);
}

////////////////////////////////////////////////////////////////////////
// secondary-specific message handlers
///////////////////////////////////////////////////////////////////////
/**
 * Confirm a pulse or CW coherent signal on a secondary beam
 */
Error
ConfirmationTask::confirmSecondarySignal(Msg *msg, Activity *act)
{
	// save the message with its signal description on the candidate list
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	secondaryList.push_back(cMsg);
	// send the signal if possible
	processNextSecondarySignal();
	return (0);
}

/**
 * Process next secondary signal
 *
 * Description:\n
 * 	If we are not currently confirming a signal, confirms the next
 * 	secondary signal in the list.
 */
void
ConfirmationTask::processNextSecondarySignal()
{
	if (!cSem->wait(0) && secondaryList.size()) {
		Msg *msg = secondaryList.front();
		secondaryList.pop_front();
		switch (msg->getCode()) {
		case SEND_CANDIDATE_PULSE_SIGNAL:
			pulseQ->send(msg);
			break;
		case SEND_CW_COHERENT_SIGNAL:
			cwQ->send(msg);
			break;
		default:
			LogFatal(ERR_IMT, activity->getActivityId(), "code = %d",
					msg->getCode());
			break;
		}
	}
}

Error
ConfirmationTask::sendCwCoherentResult(Msg *msg, Activity *act)
{
	CwCoherentSignal *cohSig = static_cast<CwCoherentSignal *> (msg->getData());

	cSem->signal();

	// find the signal
	Signal *sig = act->findCandidate(cohSig->sig.signalId);
	Debug(DEBUG_ARCHIVE, (int32_t) cohSig->sig.signalId.number, "sig id ");
	if (!sig) {

 Debug(DEBUG_ARCHIVE, (int32_t) cohSig->sig.signalId.number, "sig not found ");
#ifdef notdef
		LogError(ERR_SNC, act->getActivityId(), "sig %d", act->getActivityId(),
				cohSig->sig.signalId);
#endif
		return (0);
	}

	// set the signal state to confirmed
	sig->setState(CONFIRMED);

	// send the signal to the archive task
	MemBlk *blk = partitionSet->alloc(sizeof(Signal *));
	Assert(blk);
	Signal *signal = static_cast<Signal *> (blk->getData());
	signal = sig;
 //Debug(DEBUG_ARCHIVE, (int32_t) cohSig->sig.signalId.number, "CWDResult ");
	Msg *aMsg = msgList->alloc((DxMessageCode) CwConfirmationComplete,
			act->getActivityId(), signal, sizeof(Signal *), blk);
	archiveQ->send(aMsg);

	// if that's all the signals, send confirmation complete
	if (!act->getFirstCandidate(DETECTED))
		sendConfirmationComplete(act);
	else
		processNextSecondarySignal();
	return (0);
}

Error
ConfirmationTask::sendPulseResult(Msg *msg, Activity *act)
{
	PulseSignalHeader *pulseSig = static_cast<PulseSignalHeader *> (msg->getData());


	cSem->signal();

	// find the signal
	Signal *sig = act->findCandidate(pulseSig->sig.signalId);
	if (!sig) {
#ifdef notdef
		LogError(ERR_SNC, act->getActivityId(), "sig %d", act->getActivityId(),
				pulseSig->sig.signalId);
#endif
		return (0);
	}

	// set the signal state to confirmed
	sig->setState(CONFIRMED);

	// send the signal to the archive task
	MemBlk *blk = partitionSet->alloc(sizeof(Signal *));
	Assert(blk);
	Signal *signal = static_cast<Signal *> (blk->getData());
	signal = sig;
	Msg *aMsg = msgList->alloc((DxMessageCode) PulseConfirmationComplete,
			act->getActivityId(), signal, sizeof(Signal *), blk);
	archiveQ->send(aMsg);

	// if that's all the signals, send confirmation complete
	if (!act->getFirstCandidate(DETECTED))
		sendConfirmationComplete(act);
	else
		processNextSecondarySignal();
	return (0);
}

//
// endCwCoherentSignals: record the last of the coherent signals
//
// Notes:
//		If there are no candidates, or we're processing another
//		activity, we're done
Error
ConfirmationTask::endCwCoherentSignals(Msg *msg, Activity *act)
{
	Debug(DEBUG_CONFIRM, (void *) activity, "activity");
	Debug(DEBUG_CONFIRM, (void *) act, "act");
#ifdef notdef
	// check for matching activity
	if (activity != act)
		return (0);
#endif
	Debug(DEBUG_CONFIRM, (void *) act->getFirstCandidate(DETECTED),
			"act->getFirstCandidate");
	if (!act->getFirstCandidate(DETECTED))
		sendConfirmationComplete(act);
	else
		processNextSecondarySignal();
	return (0);
}

}
