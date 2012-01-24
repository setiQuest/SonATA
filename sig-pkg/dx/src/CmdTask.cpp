/*******************************************************************************

 File:    CmdTask.cpp
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
// Command processor task
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/CmdTask.cpp,v 1.5 2009/02/14 01:34:26 kes Exp $
//
#include <math.h>
#include <sseInterface.h>
#include <sseDxInterface.h>
#include <DxOpsBitset.h>
#include "ArchiverConnectionTask.h"
#include "CmdTask.h"
#include "CwSignal.h"
#include "Util.h"

namespace dx {

CmdTask *CmdTask::instance = 0;

CmdTask *
CmdTask::getInstance(string tname_)
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new CmdTask(tname_);
	l.unlock();
	return (instance);
}

CmdTask::CmdTask(string tname_): QTask(tname_, CMD_PRIO), controlQ(0),
		msgList(0), partitionSet(0), state(0)
{
}

CmdTask::~CmdTask()
{
}

void
CmdTask::extractArgs()
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

	CmdArgs *cmdArgs = static_cast<CmdArgs *> (args);
	Assert(cmdArgs);
	respQ = cmdArgs->respQ;
	Assert(respQ);
	controlQ = cmdArgs->controlQ;
	Assert(controlQ);

	msgList = MsgList::getInstance();
	Assert(msgList);
	partitionSet = PartitionSet::getInstance();
	Assert(partitionSet);
	state = State::getInstance();
	Assert(state);
}

void
CmdTask::handleMsg(Msg *msg)
{
	switch (msg->getCode()) {
	case REQUEST_INTRINSICS:
		sendIntrinsics(msg);
		break;
	case CONFIGURE_DX:
		configure(msg);
		break;
	case PERM_RFI_MASK:
		setPermRfiMask(msg);
		break;
	case BIRDIE_MASK:
		setBirdieMask(msg);
		break;
	case RCVR_BIRDIE_MASK:
		setRcvrBirdieMask(msg);
		break;
	case RECENT_RFI_MASK:
		setRecentRfiMask(msg);
		break;
	case TEST_SIGNAL_MASK:
		setTestSignalMask(msg);
		break;
	case REQUEST_DX_STATUS:
		sendStatus(msg);
		break;
	case SEND_DX_ACTIVITY_PARAMETERS:
		defineActivity(msg);
		break;
	case DX_SCIENCE_DATA_REQUEST:
		sendScienceData(msg);
		break;
	case BEGIN_SENDING_REQUESTED_COMPAMP_SUBCHANNELS:
		startRequestedSubchannels(msg);
		break;
	case SEND_REQUESTED_COMPAMP_SUBCHANNEL:
		sendRequestedSubchannel(msg);
		break;
	case DONE_SENDING_REQUESTED_COMPAMP_SUBCHANNELS:
		endRequestedSubchannels(msg);
		break;
	case BEGIN_SENDING_FOLLOW_UP_SIGNALS:
		startFollowUpSignals(msg);
		break;
	case SEND_FOLLOW_UP_CW_SIGNAL:
		addFollowUpCwSignal(msg);
		break;
	case SEND_FOLLOW_UP_PULSE_SIGNAL:
		addFollowUpPulseSignal(msg);
		break;
	case DONE_SENDING_FOLLOW_UP_SIGNALS:
		endFollowUpSignals(msg);
		break;
	case START_TIME:
		startActivity(msg);
		break;
	case STOP_DX_ACTIVITY:
		stopActivity(msg);
		break;
	case REQUEST_ARCHIVE_DATA:
		Debug(DEBUG_CMD, 0, "REQUEST_ARCHIVE_DATA");
		requestArchive(msg);
		break;
	case DISCARD_ARCHIVE_DATA:
		Debug(DEBUG_CMD, 0, "DISCARD_ARCHIVE_DATA");
		discardArchive(msg);
		break;
	case SHUTDOWN_DX:
		shutdown(msg);
		break;
	case RESTART_DX:
		restart(msg);
		break;
	default:
		// if we're processing secondary candidates, we have to handle
		// additional messages
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
//		DX will process secondary candidates
//		The secondary must handle signal descriptions from a primary detector;
//		these signal descriptions specify the candidate signals to be
//		confirmed.
//
Error
CmdTask::handleSecondaryMsg(Msg *msg)
{
	// find the activity; if it isn't found, just ignore the message
	Activity *act;
	if (!(act = state->findActivity(msg->getActivityId()))) {
#ifdef notdef
		LogError(ERR_NSA, msg->getActivityId(), "message = %d",
				msg->getCode());
		return (ERR_NSA);
#else
		return (0);
#endif
	}

	// get the activity parameters and make sure we are processing secondary
	// candidates
	if (!act->allowSecondaryMsg())
		return (ERR_IMT);

	Error err = 0;
	switch (msg->getCode()) {
	case BEGIN_SENDING_CANDIDATES:
		Debug(DEBUG_CMD, 0, "BEGIN_SENDING_CANDIDATES");
		err = startCandidates(msg, act);
		break;
	case SEND_CANDIDATE_CW_POWER_SIGNAL:
		Debug(DEBUG_CMD, 0, "SEND_CANDIDATE_CW_POWER_SIGNAL");
		err = addCwCandidate(msg, act);
		break;
	case SEND_CANDIDATE_PULSE_SIGNAL:
		err = addPulseCandidate(msg, act);
		break;
	case DONE_SENDING_CANDIDATES:
		Debug(DEBUG_CMD, 0,"DONE_SENDING_CANDIDATES");
		err = endCandidates(msg, act);
		break;
	case BEGIN_SENDING_CW_COHERENT_SIGNALS:
		Debug(DEBUG_CMD, 0, "BEGIN_SENDING_CW_COHERENT_SIGNALS");
		err = startCwCoherentSignals(msg, act);
		break;
	case SEND_CW_COHERENT_SIGNAL:
		Debug(DEBUG_CMD, 0, "SEND_CW_COHERENT_SIGNAL");
		err = sendCwCoherentSignal(msg, act);
		break;
	case DONE_SENDING_CW_COHERENT_SIGNALS:
		Debug(DEBUG_CMD, 0, "DONE_SENDING_CW_COHERENT_SIGNALS");
		err = endCwCoherentSignals(msg, act);
		break;
	default:
		err = ERR_IMT;
	}
	return (err);
}

void
CmdTask::sendIntrinsics(Msg *msg)
{
	MemBlk *blk = partitionSet->alloc(sizeof(DxIntrinsics));
	Assert(blk);
	DxIntrinsics *intrinsics = static_cast<DxIntrinsics *> (blk->getData());
	*intrinsics = state->getIntrinsics();
	Msg *rMsg = msgList->alloc((DxMessageCode) SEND_INTRINSICS, -1,
			intrinsics, sizeof(DxIntrinsics), blk);
	respQ->send(rMsg);
}

void
CmdTask::configure(Msg *msg)
{
	state->configure(static_cast<DxConfiguration *> (msg->getData()));
}

void
CmdTask::setPermRfiMask(Msg *msg)
{
	Debug(DEBUG_FREQ_MASK, 0, 0);
	state->setPermRfiMask(static_cast<FrequencyMaskHeader *> (msg->getData()));
}

void
CmdTask::setBirdieMask(Msg *msg)
{
	Debug(DEBUG_FREQ_MASK, 0, 0);
	state->setBirdieMask(static_cast<FrequencyMaskHeader *> (msg->getData()));
}

void
CmdTask::setRcvrBirdieMask(Msg *msg)
{
	Debug(DEBUG_FREQ_MASK, 0, 0);
	state->setRcvrBirdieMask(static_cast<FrequencyMaskHeader *>
			(msg->getData()));
}

void
CmdTask::setRecentRfiMask(Msg *msg)
{
	Debug(DEBUG_FREQ_MASK, 0, 0);
#ifdef notdef
	// create a dummy recent RFI mask
	FrequencyMaskHeader *fHdr = static_cast<FrequencyMaskHeader *> (msg->getData());
	void *tmp = malloc(sizeof(RecentRfiMaskHeader)
			+ fHdr->numberOfFreqBands * sizeof(FrequencyBand));
	RecentRfiMaskHeader *rHdr = static_cast<RecentRfiMaskHeader *> (tmp);
	rHdr->numberOfFreqBands = fHdr->numberOfFreqBands;
	rHdr->bandCovered = fHdr->bandCovered;
	FrequencyBand *fBand = reinterpret_cast<FrequencyBand *> (fHdr + 1);
	FrequencyBand *rBand = reinterpret_cast<FrequencyBand *> (rHdr + 1);
	for (int i = 0; i < fHdr->numberOfFreqBands; ++i)
		rBand[i] = fBand[i];
	state->setRecentRfiMask(rHdr);
#else
	state->setRecentRfiMask(static_cast<RecentRfiMaskHeader *> (msg->getData()));
#endif

}

void
CmdTask::setTestSignalMask(Msg *msg)
{
	Debug(DEBUG_FREQ_MASK, 0, 0);
	state->setTestSignalMask(static_cast<FrequencyMaskHeader *>
			(msg->getData()));
}

void
CmdTask::setSessionParameters(Msg *msg)
{
}

void
CmdTask::sendStatus(Msg *msg)
{
	MemBlk *blk = partitionSet->alloc(sizeof(DxStatus));
	Assert(blk);
	DxStatus *status = static_cast<DxStatus *> (blk->getData());
	*status = state->getStatus();

	Msg *rMsg = msgList->alloc(SEND_DX_STATUS, -1, status,
			sizeof(DxStatus), blk);
	respQ->send(rMsg);
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
//		Forwards the message to the control task.
// Notes:
//		Only one activity may be defined at a time.
//		Masks can be changed until the activity is
//		started with a START_TIME command.
//
void
CmdTask::defineActivity(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

/**
 * Start sending requested subchannels
 */
void
CmdTask::startRequestedSubchannels(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

void
CmdTask::sendRequestedSubchannel(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

void
CmdTask::endRequestedSubchannels(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// startFollowUpSignals: begin accepting followup signals
//
void
CmdTask::startFollowUpSignals(Msg *msg)
{
	Debug(DEBUG_CMD, (int32_t) msg->getCode(), 0);
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// startFollowUpSignals: begin accepting followup signals
//
void
CmdTask::addFollowUpCwSignal(Msg *msg)
{
	Debug(DEBUG_CMD, (int32_t) msg->getCode(), 0);
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// startFollowUpSignals: begin accepting followup signals
//
void
CmdTask::addFollowUpPulseSignal(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// startFollowUpSignals: begin accepting followup signals
//
void
CmdTask::endFollowUpSignals(Msg *msg)
{
	Debug(DEBUG_CMD, (int32_t) msg->getCode(), 0);
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// startActivity: start a previously defined activity
//
// Synopsis:
//		void startActivity(msg);
//		Msg *msg;				ptr to message
// Returns:
//		Nothing.
// Description:
//		Starts data collection for a previously defined activity.
// Notes:
//		This method changes the activity state to DX_ACT_PEND_DC; it is
//		done here to avoid a race condition with CollectionTask.
//		Only one activity may be in data collection at a time.
//		After an activity is started, none of its masks may be changed.
//
void
CmdTask::startActivity(Msg *msg)
{
	const SseInterfaceHeader& hdr = msg->getHeader();
	Assert(hdr.dataLength == sizeof(StartActivity));

	// make sure there is no other activity starting or in data collection
	Activity *act;
	if ((act = state->findActivity(DX_ACT_PEND_BASE_ACCUM))
			|| (act = state->findActivity(DX_ACT_RUN_BASE_ACCUM))
			|| (act = state->findActivity(DX_ACT_PEND_DC))
			|| (act = state->findActivity(DX_ACT_RUN_DC))) {
		LogError(ERR_AIC, act->getActivityId(),
				"activity %d already collecting, can't start %d",
				act->getActivityId(), hdr.activityId);
		return;
	}

	// notify the control task
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// sendScienceData: set the science data request
void
CmdTask::sendScienceData(Msg *msg)
{
	DxScienceDataRequest *scienceData = static_cast<DxScienceDataRequest *>
			(msg->getData());
	SseInterfaceHeader hdr = msg->getHeader();
	if (hdr.activityId >= 0) {
		Activity *act = state->findActivity(hdr.activityId);
		if (act)
			act->setScienceData(scienceData);
		else {
			LogWarning(ERR_NSA, hdr.activityId,
					"requesting science data for activity %d", hdr.activityId);
		}
	}
	else {
		// lock the state structure until we're done updating all
		// current activities
		state->lock();
		for (Activity *act = state->getFirstActivity(); act;
				act = state->getNextActivity()) {
			act->setScienceData(scienceData);
		}
		state->unlock();
	}
}

void
CmdTask::stopActivity(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// requestArchive: archive the candidate signal before releasing it
//
// Notes:
//		The message is forwarded to the control task.
//
void
CmdTask::requestArchive(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

//
// discardArchive: release the candidate signal without archiving
//
void
CmdTask::discardArchive(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

void
CmdTask::shutdown(Msg *msg)
{
	::exit(0);
	Debug(DEBUG_ALWAYS, 0, "after shutdown - calling alloc");
	Msg *cMsg = msgList->alloc();
	Debug(DEBUG_ALWAYS, 0, "after shutdown - calling forward ");
	msg->forward(cMsg);
	Debug(DEBUG_ALWAYS, 0, "after shutdown - calling send");
	controlQ->send(cMsg);
}

void
CmdTask::restart(Msg *msg)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
}

////////////////////////////////////////////////////////////////////////
// secondary-specific message handlers
///////////////////////////////////////////////////////////////////////

//
// startCandidates: start receiving candidates
//
Error
CmdTask::startCandidates(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
	return (0);
}

//
// addCwCandidate: add a CW candidate signal for confirmation
//
// Notes:
//		A CwPowerSignal structure is sent to describe the signal.
//
Error
CmdTask::addCwCandidate(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
	return (0);
}

Error
CmdTask::addPulseCandidate(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
	return (0);
}

//
// endCandidates: done receiving candidates
//
// Notes:
//		Send a message to the SSE with the count of candidates to
//		be processed.
//
Error
CmdTask::endCandidates(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
	return (0);
}

//
// startCWCoherentSignals: send the cw coherent signal count
//		to the control task
Error
CmdTask::startCwCoherentSignals(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
	return (0);
}

//
// Notes:
//		Process a CW coherent signal from a primary detector.  A message
//		is sent to the confirmation task to start the confirmation of
//		the signal.
//
Error
CmdTask::sendCwCoherentSignal(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
	return (0);
}

Error
CmdTask::endCwCoherentSignals(Msg *msg, Activity *act)
{
	Msg *cMsg = msgList->alloc();
	msg->forward(cMsg);
	controlQ->send(cMsg);
	return (0);
}

}
