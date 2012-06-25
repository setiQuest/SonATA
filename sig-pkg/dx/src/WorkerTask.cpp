/*******************************************************************************

 File:    WorkerTask.cpp
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
// Worker task: create subchannels, then call spectrometer to create spectra
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/WorkerTask.cpp,v 1.10 2009/05/24 23:06:25 kes Exp $
//

#include "ATADataPacketHeader.h"
#include "DxErr.h"
#include "Log.h"
#include "WorkerTask.h"
#include "CollectionTask.h"

namespace dx {

WorkerTask::WorkerTask(string name_): QTask(name_, WORKER_PRIO),
		unit(UnitWorker), activityId(-1), sampleBufSize(0), sampleBuf(0),
		activity(0), channel(0), msgList(0), collectionQ(0), spectrometer(0)
{
	for (int32_t i = 0; i < MAX_SUBCHANNELS; ++i)
		rOut[i] = lOut[i] = 0;
}

WorkerTask::~WorkerTask()
{
}

/**
 * Initialize the task.
 *
 * Description:\n
 * 	Called once when the system is started.  Allocates data as necessary.
 */
void
WorkerTask::extractArgs()
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

	// process the startup args
	WorkerArgs *workerArgs = static_cast<WorkerArgs *> (args);
	Assert(workerArgs);
	unit = workerArgs->unit;
	collectionQ = workerArgs->collectionQ;
	Assert(collectionQ);

	msgList = MsgList::getInstance();
	Assert(msgList);
	spectrometer = Spectrometer::getInstance();
	Assert(spectrometer);
	Queue *workQ = WorkQ::getInstance();
	Assert(workQ);
	setInputQueue(workQ);

	state = State::getInstance();
	Assert(state);
}

/**
 * Set up the worker task and start an activity.
 *
 * Description:\n
 * 	Sets up the worker task for a new data collection.  The sample buffer
 * 	is allocated if the existing buffer is too small.
 * 	This serves as a "start collection" for the worker.
 */
void
WorkerTask::startActivity(Activity *act)
{
	Assert(act);
	activity = act;
	channel = activity->getChannel();
	Assert(channel);

	lock();
	spectrometer->stopSpectrometry(activity);
	activityId = activity->getActivityId();
	// allocate a working buffer for the sample input; data will be transferred
	// into this buffer before performing the DFB
	// if a smaller buffer has already been allocated, free it
	uint32_t size = channel->getThreshold() * sizeof(ComplexFloat32);
	if (sampleBuf && sampleBufSize < size) {
		fftwf_free(sampleBuf);
		sampleBuf = 0;
	}
	if (!sampleBuf) {
		sampleBuf = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
		sampleBufSize = size;
	}
	Assert(sampleBuf);

	// initialize the spectrometer
	spectrometer->setup(activity);
	unlock();
}

/**
 * Stop the activity.
 *
 * Description:\n
 * 	Sets up the worker task for a new data collection.  The sample buffer
 * 	is allocated if the existing buffer is too small.
 */
void
WorkerTask::stopActivity(Activity *act)
{
	// make sure we're doing this activity, then stop it if we are
	if (activity && activity == act) {
		lock();
		spectrometer->stopSpectrometry(activity);
		activity = 0;
		activityId = -1;
		unlock();
	}
}

void
WorkerTask::handleMsg(Msg *msg)
{
	// flush any irrelevant messages
	if (msg->getActivityId() != activityId)
		return;

	switch (msg->getCode()) {
	case DfbProcess:
		// perform a DFB/channelization
		lock();
		processData(msg);
		unlock();
		break;
	case DATA_COLLECTION_COMPLETE:
		// all channels are done, time to go
		lock();
		spectrometer->stopSpectrometry(activity);
		activity = 0;
		activityId = -1;
		unlock();
		break;
	case STOP_DX_ACTIVITY:
	case SHUTDOWN_DX:
	case RESTART_DX:
		lock();
		stopActivity(msg);
		unlock();
		terminate();
		break;
	default:
		Fatal(ERR_IMT);
		break;
	}
}

/**
* process input data
*
* Description:\n
*	Performs a DFB operation on a block of data from the
*	input packet handler.  An entire half-frame of output data
*	is produced for both polarizations; the data is then passed
*	to the spectrometer with a call.\n
* Notes:\n
*	The buffers are owned by the channel.
*
* @param	msg a pointer to the message, which contains a pointer to
*				the half frame information.
*/
//
// processData: process channel data
//
// Notes:
//		Perform a DFB and subchannelization of the specified channel.
//		Data is placed in a half-frame buffer, which is then passed
//		to the spectrometry task for spectrum analysis.
//
void
WorkerTask::processData(Msg *msg)
{
	// get the channel
	HalfFrameInfo *hfInfo = static_cast<HalfFrameInfo *> (msg->getData());
	Assert(hfInfo);

	// if not collecting data, just flush the data without doing a DFB,
	// to ensure that the input buffer does not fill up.
	if (!activity) {
		channel->dfbFlush(hfInfo->sample);
		return;
	}

#if WORKER_TIMING
	uint64_t t0 = getticks();
#endif
	// find a pair of half frame buffers
	BufPair *hfBuf = channel->allocHfBuf();
	if (!hfBuf)
		Fatal(ERR_NBA);
	ComplexFloat32 *rBuf = static_cast<ComplexFloat32 *>
			(hfBuf->getBufData(ATADataPacketHeader::RCIRC));
	ComplexFloat32 *lBuf = static_cast<ComplexFloat32 *>
			(hfBuf->getBufData(ATADataPacketHeader::LCIRC));

	// build the array of output pointers
	buildOutputArray(rOut, rBuf, channel->getSamplesPerSubchannelHalfFrame(),
			channel->getTotalSubchannels(), channel->getUsableSubchannels());
	buildOutputArray(lOut, lBuf, channel->getSamplesPerSubchannelHalfFrame(),
			channel->getTotalSubchannels(), channel->getUsableSubchannels());
#if WORKER_TIMING
	uint64_t t1 = getticks();
#endif

	// do the DFB and count the half frame
	channel->dfbProcess(hfInfo->sample, sampleBuf, rOut, lOut);
#if WORKER_TIMING
	uint64_t t2 = getticks();
#endif
	// call the spectrometer to compute the spectra
	SpecState s = spectrometer->processHalfFrame(channel->getHalfFrame(),
			hfBuf);
	// handle state changes
	switch (s) {
	case BLStarted:
		startBaseline();
		break;
	case DCStarted:
		startDataCollection();
		break;
	case DCComplete:
		completeDataCollection();
		break;
	default:
		break;
	}
#if WORKER_TIMING
	uint64_t t3 = getticks();
	++timing.iterations;
	timing.buildArrays += elapsed(t1, t0);
	timing.dfb += elapsed(t2, t1);
	timing.spectrometer += elapsed(t3, t2);
	timing.total += elapsed(t3, t0);
#endif
}

/**
 * Notify the collection task that baselining has started.
 *
 * Notes:\n
 * 	If there is no baselining, then we go straight to data collection, but
 * 	send the appropriate messages.
 */
void
WorkerTask::startBaseline()
{
	Msg *cMsg = msgList->alloc(BASELINE_INIT_ACCUM_STARTED,
			activity->getActivityId());
	cMsg->setUnit((sonata_lib::Unit) unit);
	collectionQ->send(cMsg);
}

/**
 * Notify the collection task that data collection has started.
 */
void
WorkerTask::startDataCollection()
{
	// register the end of baseline accumulation
	Msg *cMsg = msgList->alloc(BASELINE_INIT_ACCUM_COMPLETE,
			activity->getActivityId());
	cMsg->setUnit((sonata_lib::Unit) unit);
	collectionQ->send(cMsg);

	// go straight into data collection.  [NOTE: if we want to restart
	// buffering when we start data collection, we should flush the
	// channel input buffers and set the state to PEND_DC.
	activity->setState(DX_ACT_RUN_DC);
	activity->setState(DX_ACT_PEND_DC);
	// send start of data collection
	cMsg = msgList->alloc(DATA_COLLECTION_STARTED, activity->getActivityId());
	cMsg->setUnit((sonata_lib::Unit) unit);
	collectionQ->send(cMsg);
}

/**
 * Notify the collection task that data collection is complete.
 */
void
WorkerTask::completeDataCollection()
{
	// done with data collection; report completion to collection task
	// and any packet reception problems to SSE.
	ChannelStatistics stats;
	channel->getStats(stats);
	Debug(DEBUG_ALWAYS, (int32_t) stats.netStats.total,
			"total packets");
	Debug(DEBUG_ALWAYS, (int32_t) stats.netStats.missed,
			"missed packets");
	Debug(DEBUG_ALWAYS, (int32_t) stats.netStats.late,
			"late packets");
	if ((stats.netStats.missed + stats.netStats.late) > 0 && activity) {
		LogWarning(ERR_LMP, activity->getActivityId(),
				"channel %d, total packets = %d, missed = %d, late = %d",
				channel->getChannelNum(), stats.netStats.total,
				stats.netStats.missed, stats.netStats.late);
	}
	Msg *cMsg = msgList->alloc(DATA_COLLECTION_COMPLETE, activityId,
			reinterpret_cast<void *> (channel->getChannelNum()), 0, 0,
			IMMEDIATE);
	cMsg->setUnit((sonata_lib::Unit) unit);
	collectionQ->send(cMsg);
	stopActivity(activity);
}

/**
 * Stop the activity.
 *
 * Description:\n
 * 	A message has been received to abort the activity.  This could be a
 * 	simple stop activity, a full shutdown of the detector or a restart of
 * 	the detector.
 */
void
WorkerTask::stopActivity(Msg *msg)
{
	if (activityId == msg->getActivityId())
		spectrometer->stopSpectrometry(activity);
}

/**
* Build the output array for the DFB data.
*
* Description:\n
*	Builds an array of pointers to the half-frame buffers.  The pointers
*	are initialized so that the lowest frequency is in index 0, the center
*	frequency is in index usable / 2 and the highest frequency is in
*	index usable - 1.
*/
void
WorkerTask::buildOutputArray(ComplexFloat32 **array, ComplexFloat32 *buf,
		int32_t samples, int32_t subchannels, int32_t usable)
{
	int32_t h = usable / 2;
	int32_t d = subchannels - usable / 2;
	for (int32_t i = 0; i < subchannels; ++i) {
		uint32_t ofs;
		if (i < h)
			ofs = channel->getHfOfs(i + h);
		else if (i >= d)
			ofs = channel->getHfOfs(i - d);
		else
			ofs = channel->getHfOfs(usable);
		array[i] = &buf[ofs];
	}
}

}
