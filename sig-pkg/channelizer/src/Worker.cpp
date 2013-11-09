/*******************************************************************************

 File:    Worker.cpp
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
// Ethernet worker task
//
// $Header: /home/cvs/nss/sonata-pkg/channelizer/src/Worker.cpp,v 1.28 2009/02/13 18:11:55 kes Exp $
//

#include "Worker.h"
#include "Args.h"

namespace chan {

Queue *TransmitterQ::instance = 0;
Queue *WorkQ::instance = 0;

WorkerTask::WorkerTask(string name_): QTask(name_, WORKER_PRIO, true, false),
		messages(0), dfbId(-1), unit(UnitChannelize), workQ(0),
		sampleBuf(0), buf(0), beam(0), vectorList(0), msgList(0),
		transmitterQ(0)
{
}

WorkerTask::~WorkerTask()
{
}

void
WorkerTask::extractArgs()
{
	//JR - nov 05, 2013 - Added to support starting worker thread at a certain CPU
	Args *cmdLineArgs = Args::getInstance();
	int firstCPU = cmdLineArgs->getWorkersFirstCPU();
#if ASSIGN_CPUS
	// if there are more than 3 processors, keep off first 3
	int32_t nCpus = sysconf(_SC_NPROCESSORS_CONF);
	int64_t n = (int64_t) args;
	if (nCpus > firstCPU) {
		// set the affinity to cpu 0
		cpu_set_t affinity;
		CPU_ZERO(&affinity);
		//JR - Changed 3 to firstCPU
		if (n >= 0 && nCpus > n + firstCPU)
	        {
			printf("WorkerThread %d is on CPU %d\n", (int)n+1, (int)n + firstCPU);
			CPU_SET(n + firstCPU, &affinity);
		}
		else {
			for (int32_t i = firstCPU; i < nCpus; ++i)
			{
				printf("WorkerThread %d is on CPU %d\n", (int)n+1, i);
				CPU_SET(i, &affinity);
			}
		}
		pid_t tid = gettid();
		int rval = sched_setaffinity(tid, sizeof(cpu_set_t), &affinity);
		Assert(rval >= 0);
	}
#endif

	// record the original task priority
	beam = Beam::getInstance();
	Assert(beam);
	msgList = MsgList::getInstance();
	Assert(msgList);
	vectorList = ChannelPacketVectorList::getInstance();
	Assert(vectorList);
	workQ = WorkQ::getInstance();
	Assert(workQ);
	setInputQueue(workQ);
	transmitterQ = TransmitterQ::getInstance();
	Assert(transmitterQ);

	// allocate a dfb for channelization
	dfbId = beam->allocDfb();

	// allocate a working buffer for the sample input; the data will be
	// transferred to this buffer before performing the DFB
	uint32_t size = beam->getThreshold() * sizeof(ComplexFloat32);
	sampleBuf = (ComplexFloat32 *) fftwf_malloc(size);
	Assert(sampleBuf);

	// allocate a large DFB buffer
	size = MAX_TOTAL_CHANNELS * ATADataPacketHeader::CHANNEL_SAMPLES
			* sizeof(ComplexFloat32);
	buf = (ComplexFloat32 *) fftwf_malloc(size);
	Assert(buf);

	// create the arrays of pointers
	buildOutputArray(outArray, buf, ATADataPacketHeader::CHANNEL_SAMPLES,
			beam->getTotalChannels(), beam->getUsableChannels());
	buildPacketArray(pktArray, buf, ATADataPacketHeader::CHANNEL_SAMPLES,
			beam->getTotalChannels(), beam->getUsableChannels());
}

void
WorkerTask::handleMsg(Msg *msg)
{
	switch (msg->getCode()) {
	case DfbChannel:
		// perform a DFB/channelization
		++messages;
		processData(msg);
		break;
	default:
		Fatal(ERR_IMT);
		break;
	}
}

/**
* Process input data
*
* Description:\n
*	Performs a DFB operation on a block of data from the
*	input packet handler.  An entire packet of output data
*	is produced.  The active channels are then transmitted
* 	to the appropriate multicast addresses.\n
* Notes:\n
*	None.
*
* @param	msg a pointer to the message, which contains a pointer to
*			the channel.
*/
//
// processData: process channel data
//
// Notes:
//		Perform a DFB and channelization of the specified channel.
//		Data is placed in packet buffers, which are then passed
//		to the transmitter for output.
//
void
WorkerTask::processData(Msg *msg)
{
	// if we're not channelizing, just ignore the request
	if (beam->getState() != STATE_RUNNING)
		return;

	// get the channel
	PacketInfo *pktInfo = static_cast<PacketInfo *> (msg->getData());
	Assert(pktInfo);

	// do the DFB
#if (WORKER_TIMING)
	uint64_t t0 = getticks();
#endif
	beam->dfbProcess(dfbId, pktInfo->sample, sampleBuf, outArray);

	// convert the output data from floating point to integer packet format
#if (WORKER_TIMING)
	uint64_t t1 = getticks();
#endif
#ifndef INPUT_ONLY
	ChannelPacketVector *pktOut = createPacketVector(pktInfo, pktArray);

#if (WORKER_TIMING)
	uint64_t t2 = getticks();
#endif
	// send the packet vector as a message to the transmitter
	Msg *tMsg = msgList->alloc((DxMessageCode) OutputVector, 0, pktOut,
		sizeof(ChannelPacketVector *), 0, USER);
	Assert(tMsg);
	tMsg->setMsgNumber(pktInfo->hdr.seq);

	Assert(!transmitterQ->send(tMsg, 0));
#if (WORKER_TIMING)
	uint64_t t3 = getticks();
	++timing.iterations;
	timing.dfb += elapsed(t1, t0);
	timing.createVector += elapsed(t2, t1);
	timing.transmit += elapsed(t3, t2);
	timing.total += elapsed(t3, t0);
#endif
#endif // INPUT_ONLY
}

/**
* Build the output array for the DFB data.
*
* Description:\n
*	Builds an array of pointers to the channel buffers.  The pointers
*	are initialized so that the lowest frequency is in index 0 and the
*	center frequency is in index nChannels / 2.
*/
void
WorkerTask::buildOutputArray(ComplexFloat32 **array, ComplexFloat32 *buf,
		int32_t samples, int32_t channels, int32_t usable)
{
#ifdef notdef
	int32_t i, j;
	for (i = 0, j = nChannels / 2; i < channels / 2; ++i, ++j) {
		array[i] = buf + j * samples;
		array[j] = buf + i * samples;
	}
#else
	for (int32_t i = 0; i < channels; ++i) {
		if (i < (usable + 1) / 2 || i >= channels - usable / 2)
			array[i] = buf + i * samples;
		else
			array[i] = buf + usable * samples;
	}
#endif
}

/**
 * Build the output array for packets.
 *
 * Description:\n
 * 	Builds the array of channel output data; the channels are contiguous
 * 	in memory.
 *
 * @param		array the array of pointers to the channel data.
 * @param		buf buffer containing the channel data.
 * @param		samples number of samples per channel.
 * @param		nChannels number of channels.
 */
void
WorkerTask::buildPacketArray(ComplexFloat32 **array, ComplexFloat32 *buf,
		int32_t samples, int32_t channels, int32_t usable)
{
	int32_t i = 0;
	// store the negative frequencies
	for (int32_t j = channels - usable / 2; i < usable / 2; ++i, ++j)
		array[i] = buf + j * samples;
	// now DC and the positive frequencies
	for(int32_t j = 0; j < (usable + 1) / 2; ++i, ++j)
		array[i] = buf + j * samples;
}

/**
 * Build the channel packets for output.
 *
 * Description:\n
 * 	Allocates channel packets, initializes them and stores them
 * 	in a vector for transmission.\n
 * Notes:\n
 * 	The data returned by the DFB is single precision floating point,
 * 	so it must be converted to 16-bit integer packet format.
 */
ChannelPacketVector *
WorkerTask::createPacketVector(PacketInfo *pktInfo, ComplexFloat32 **out)
{
	int32_t ctr = pktInfo->usableChannels / 2;
	float64_t chanSpacing = pktInfo->bandwidth / pktInfo->totalChannels;

#if (WORKER_TIMING)
		uint64_t t0 = getticks();
#endif
	ChannelPacketVector *v = vectorList->alloc();
	Assert(v);
#if (WORKER_TIMING)
		uint64_t t1 = getticks();
#endif
	for (int32_t i = 0; i < pktInfo->usableChannels; ++i) {
#if (WORKER_TIMING)
		uint64_t t2 = getticks();
#endif
		ChannelPacket *pkt = &(*v)[i];
		ATADataPacketHeader& hdr = pkt->getHeader();
		hdr = pktInfo->hdr;
		hdr.chan = i;
		hdr.freq += (i - ctr) * chanSpacing;
//		pkt->putHeader(hdr);
#if (WORKER_TIMING)
		uint64_t t3 = getticks();
#endif
#ifdef notdef
		ComplexFloat32 *data = out[i];
		for (int32_t j = 0; j < hdr.len; ++j)
			pkt->putSample(j, data[j]);
#else
		pkt->putSamples(out[i]);
#endif
#if (WORKER_TIMING)
		uint64_t t4 = getticks();
		++timing.vector.creates;
		timing.vector.allocate += elapsed(t1, t0);
		timing.vector.putHeader += elapsed(t3, t2);
		timing.vector.putSamples += elapsed(t4, t3);
		timing.vector.total += elapsed(t4, t2);
#endif
	}
	return (v);
}

}
