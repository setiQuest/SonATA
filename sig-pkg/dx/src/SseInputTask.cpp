/*******************************************************************************

 File:    SseInputTask.cpp
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
// SSE->DX input handler task
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/SseInputTask.cpp,v 1.6 2009/07/12 18:53:28 kes Exp $
//

#include <ssePorts.h>
#include "DxErr.h"
//#include "DxStruct.h"
#include "Log.h"
#include "PulseFollowupSignal.h"
//#include "SseBroadcastTask.h"
#include "SseInputTask.h"
#include "SseOutputTask.h"
#include "Timer.h"
#include <fstream>

namespace dx {

SseInputTask *SseInputTask::instance = 0;

SseInputTask *
SseInputTask::getInstance(string tname_)
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new SseInputTask(tname_);
	l.unlock();
	return (instance);
}

SseInputTask::SseInputTask(string tname_): Task(tname_, SSE_INPUT_PRIO),
		sse(0), connectionQ(0), cmdQ(0), cmdArgs(0), msgList(0), partitionSet(0)

{
}

SseInputTask::~SseInputTask()
{
}

/**
 * Perform initialization, processing any arguments which are passed
 * to the task.  Executed when the task begins to run.
 */
void
SseInputTask::extractArgs()
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

	// extract the input args
	SseInputArgs *inputArgs = static_cast<SseInputArgs *> (args);
	Assert(inputArgs);
	sse = inputArgs->sse;
	Assert(sse);
	connectionQ = inputArgs->connectionQ;
	Assert(connectionQ);
	cmdQ = inputArgs->cmdQ;
	Assert(cmdQ);

	cmdArgs = Args::getInstance();
	Assert(cmdArgs);
	msgList = MsgList::getInstance();
	Assert(msgList);
	partitionSet = PartitionSet::getInstance();
	Assert(partitionSet);
}

//
// routine: receive and send on incoming messages
//
// Notes:
//		No analysis of the message is performed - that is left
//		to the command processor task.  Instead, the local copy
//		of the header
//		is demarshalled to determined whether or not there
//		is associated data; if there is, space is allocated to
//		contain it.
void *
SseInputTask::routine()
{
	extractArgs();

	// run forever, waiting for messages from the SSE
	bool stopIssued = false;
	bool done = false;
	uint32_t lastCode = MESSAGE_CODE_UNINIT;
	int32_t lastLen = 0;
	uint32_t lastTime = 0;
	while (!done) {
		// if there's no connection, request that it be
		// established, then wait for that to happen
		if (!sse->isConnected()) {
			requestConnection();
			while (!sse->isConnected()) {
				Timer timer;
				timer.sleep(3000);
			}
		}
		stopIssued = false;

		// got a connection - wait for data to come in
		SseInterfaceHeader hdr;
		Error err = sse->recv((void *) &hdr, sizeof(hdr));
		if (err) {
			switch (err) {
			case EAGAIN:
			case EINTR:
			case ENOTCONN:
			case ECONNRESET:
				stopAllActivities(stopIssued);
				continue;
			default:
				Fatal(err);
				break;
			}
		}
		// demarshall the header
		hdr.demarshall();

		if (cmdArgs->logSseMessages()) {
			LogWarning(ERR_NE, hdr.activityId,
					"bad msg from Sse, code = %d, len = %d", hdr.code,
					hdr.dataLength);
		}

		// allocate a message to hold the incoming message
		Msg *msg = msgList->alloc();
		msg->setHeader(hdr);
		msg->setUnit((sonata_lib::Unit) UnitSse);

		// if there's data associated with the message,
		// allocate space and retrieve it, demarshall it
		// based on the message type,
		// then send it on to the command processor
		void *data = 0;
		int32_t len = hdr.dataLength;
		timeval tv;
		gettimeofday(&tv, NULL);
		if (len > 10000) {
			LogWarning(ERR_NE, hdr.activityId,
					"msg code = %d, len = %d, t = %u, last msg = %d, last len = %d, last t = %u",
					hdr.code, len, tv.tv_sec, lastCode, lastLen, lastTime);
			Timer t;
			t.sleep(100);
		}
		else {
			lastCode = hdr.code;
			lastLen = len;
			lastTime = tv.tv_sec;
		}
		if (len) {
			MemBlk *blk = partitionSet->alloc(len);
			Assert(blk);
			if (!blk)
				Fatal(ERR_MAF);
			data = blk->getData();
			err = sse->recv(data, len);
			if (err) {
				switch (err) {
				case EAGAIN:
				case EINTR:
				case ENOTCONN:
				case ECONNRESET:
					blk->free();
					Assert(msgList->free(msg));
					stopAllActivities(stopIssued);
					continue;
				default:
					Fatal(err);
					break;
				}
			}
			msg->setData(data, len, blk);
		}

		// demarshall the data of the message depending on
		// the message type
		switch (hdr.code) {
		case REQUEST_INTRINSICS:
			break;
		case CONFIGURE_DX:
			(static_cast<DxConfiguration *> (data))->demarshall();
			break;

		case PERM_RFI_MASK:
		case BIRDIE_MASK:
		case RCVR_BIRDIE_MASK:
		case TEST_SIGNAL_MASK:
			demarshallFrequencyMask(data);
			break;
		case RECENT_RFI_MASK:
			demarshallRecentRFIMask(data);
			break;
		case REQUEST_DX_STATUS:
			break;
		case SEND_DX_ACTIVITY_PARAMETERS:
			(static_cast<DxActivityParameters *> (data))->demarshall();
			break;
		case DX_SCIENCE_DATA_REQUEST:
			(static_cast<DxScienceDataRequest *> (data))->demarshall();
			break;
		case BEGIN_SENDING_REQUESTED_COMPAMP_SUBCHANNELS:
			(static_cast<Count *> (data))->demarshall();
			break;
		case SEND_REQUESTED_COMPAMP_SUBCHANNEL:
			(static_cast<DxScienceData *> (data))->demarshall();
			break;
		case DONE_SENDING_REQUESTED_COMPAMP_SUBCHANNELS:
			break;
		case BEGIN_SENDING_FOLLOW_UP_SIGNALS:
			(static_cast<Count *> (data))->demarshall();
			break;
		case SEND_FOLLOW_UP_CW_SIGNAL:
			(static_cast<FollowUpCwSignal *> (data))->demarshall();
			break;
		case SEND_FOLLOW_UP_PULSE_SIGNAL:
			(static_cast<FollowUpPulseSignal *> (data))->demarshall();
			break;
		case DONE_SENDING_FOLLOW_UP_SIGNALS:
			break;
		case START_TIME:
			(static_cast<StartActivity *> (data))->demarshall();
			break;
		case BEGIN_SENDING_CANDIDATES:
			(static_cast<Count *> (data))->demarshall();
			break;
		case SEND_CANDIDATE_CW_POWER_SIGNAL:
			(static_cast<CwPowerSignal *> (data))->demarshall();
			break;
		case SEND_CANDIDATE_PULSE_SIGNAL:
			demarshallPulseSignal(data);
			break;
		case DONE_SENDING_CANDIDATES:
			break;
		case BEGIN_SENDING_CW_COHERENT_SIGNALS:
			break;
		case SEND_CW_COHERENT_SIGNAL:
			(static_cast<CwCoherentSignal *> (data))->demarshall();
			break;
		case DONE_SENDING_CW_COHERENT_SIGNALS:
			break;
		case REQUEST_ARCHIVE_DATA:
			(static_cast<ArchiveRequest *> (data))->demarshall();
			break;
		case DISCARD_ARCHIVE_DATA:
			(static_cast<ArchiveRequest *> (data))->demarshall();
			break;
		// the following commands arrive with no data
		case STOP_DX_ACTIVITY:
		case SHUTDOWN_DX:
		case RESTART_DX:
			Debug(DEBUG_CONTROL, hdr.activityId,
					"STOP_DX_ACTIVITY, act");
			break;
		default:
			LogError(ERR_IMT, hdr.activityId, "activity %d, type %d",
					hdr.activityId, hdr.code);
			Err(ERR_IMT);
			ErrStr(hdr.code, "msg code");
			Assert(msgList->free(msg));
			continue;
		}

		// at this point, the entire marshalled message is in
		// a generic Msg; send the message on for processing,
		// then go back to waiting
		cmdQ->send(msg);
	}
	return (0);
}

//
// requestConnection: request that a connection be established to
//		the SSE
//
void
SseInputTask::requestConnection()
{
	Msg *msg = msgList->alloc((DxMessageCode) InitiateConnection);
	if (connectionQ->send(msg))
		Fatal(ERR_SE);
}

//
// demarshallFrequencyMask: demarshall a Frequency Mask
//
void
SseInputTask::demarshallFrequencyMask(void *data)
{
	FrequencyMaskHeader *hdr = static_cast<FrequencyMaskHeader *> (data);
	hdr->demarshall();
	FrequencyBand *array = reinterpret_cast<FrequencyBand *> (hdr + 1);
	for (int32_t i = 0; i < hdr->numberOfFreqBands; ++i)
		array[i].demarshall();
}

//
// demarshallRecentRfiMask: demarshall a recent RFI Frequency Mask
//
void
SseInputTask::demarshallRecentRFIMask(void *data)
{
	RecentRfiMaskHeader *hdr = static_cast<RecentRfiMaskHeader *> (data);
	hdr->demarshall();
	FrequencyBand *array = reinterpret_cast<FrequencyBand *> (hdr + 1);
	for (int32_t i = 0; i < hdr->numberOfFreqBands; ++i)
		array[i].demarshall();
}

void
SseInputTask::demarshallPulseSignal(void *data)
{
	PulseSignalHeader *hdr = static_cast<PulseSignalHeader *> (data);
	hdr->demarshall();
	::Pulse *array = reinterpret_cast< ::Pulse *> (hdr + 1);
	for (int i = 0; i < hdr->train.numberOfPulses; ++i) {
		array[i].demarshall();
	}
}

//
// stopAllActivities: we just lost our connection; stop all activities
//
void
SseInputTask::stopAllActivities(bool& stopIssued)
{
	if (!stopIssued) {
		Msg *msg = msgList->alloc(STOP_DX_ACTIVITY, NSS_NO_ACTIVITY_ID);
		cmdQ->send(msg);
	}
	stopIssued = true;
}

}
