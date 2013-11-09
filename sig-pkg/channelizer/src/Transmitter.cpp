/*******************************************************************************

 File:    Transmitter.cpp
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
// TransmitterTask task
//
// $Header: /home/cvs/nss/sonata-pkg/channelizer/src/Transmitter.cpp,v 1.25 2009/05/24 22:25:42 kes Exp $
//
#include "Transmitter.h"
#include "Args.h"

using std::norm;

namespace chan {

TransmitterTask *TransmitterTask::instance = 0;

/**
 * Packet transmitter; singleton class.
 *
 * Description:\n
 *	Controls output of the channelizer by serializing packets to
 * 	ensure that they go out in sequential order.
*/

TransmitterTask *
TransmitterTask::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new TransmitterTask("Transmitter", TRANSMITTER_PRIO);
	l.unlock();
	return (instance);
}

TransmitterTask::TransmitterTask(string name_, int prio_):
		QTask(name_, prio_, true, false), abort(false),
		curSeq(0), code(ATADataPacketHeader::XLINEAR), unit(UnitTransmit),
		connection(0), beam(0), vectorList(0), msgList(0),
		transmitterQ(0)
{
}

TransmitterTask::~TransmitterTask()
{
}

void
TransmitterTask::extractArgs()
{
#if ASSIGN_CPUS
	//JR - Nov 08, 2013 - Added support for optionally specifying CPU.
	Args *commandLineArgs = Args::getInstance();
	int cpu = commandLineArgs->getTransmitterCPU();
	// on multiprocessor systems, park on cpu 1
	int32_t nCpus = sysconf(_SC_NPROCESSORS_CONF);
	if (nCpus > 3) {
		// set the affinity to cpu 3
		cpu_set_t affinity;
		CPU_ZERO(&affinity);
		CPU_SET(cpu, &affinity);
		printf("Transmiiter CPU=%d\n", cpu);
		pid_t tid = gettid();
		int rval = sched_setaffinity(tid, sizeof(cpu_set_t), &affinity);
		Assert(rval >= 0);
	}
#endif

	beam = Beam::getInstance();
	Assert(beam);
	vectorList = ChannelPacketVectorList::getInstance();
	Assert(vectorList);
	msgList = MsgList::getInstance();
	Assert(msgList);
	transmitterQ = TransmitterQ::getInstance();
	Assert(transmitterQ);
	setInputQueue(transmitterQ);

	const HostSpec& output = beam->getOutput();
	base = output;
	inet_aton(base.addr, &base.inaddr);

	connection = new Udp(std::string("transmit"),
			(sonata_lib::Unit) UnitTransmit);
	if (!connection)
		Fatal(ERR_CCC);
	connection->setAddress(output.addr, output.port);
	// set the buffer size
	size_t size = DEFAULT_SND_BUFSIZE;
	connection->setSndBufsize(size);
}

void
TransmitterTask::handleMsg(Msg *msg)
{
	switch (msg->getCode()) {
	case OutputVector:
		transmit(msg);
		break;
	default:
		Fatal(ERR_IMT);
		break;
	}
}

/**
 * Transmit a set of channel data packets.
 *
 * Description:\n
 * 	Transmits all data packets for a single iteration of the
 * 	channelization process.  Channels are transmitted in order.\n\n
 *
 * Notes:\n
 * 	There may be more than one task performing channelization, so there
 * 	is no guarantee that channel packet vectors will be received by this
 * 	function in strict order.  It is therefore necessary to force
 * 	sequential output order by queuing vectors which arrive early.
 */
void
TransmitterTask::transmit(Msg *msg)
{
	// if channelization has been stopped, ignore the message
	if (beam->getState() != STATE_RUNNING) {
		restart();
		return;
	}
#if TRANSMITTER_TIMING
	++timing.vectors;
	uint64_t t0 = getticks();
#endif
	int32_t seq = msg->getMsgNumber();
	ChannelPacketVector *vector = static_cast<ChannelPacketVector *>
			(msg->getData());
	// must be data to transmit
	if (vector->empty())
		Fatal(ERR_ECL);
#if TRANSMITTER_TIMING
	uint64_t t1 = getticks();
#endif
	// test for sequential output
	if (seq != curSeq) {
#if TRANSMITTER_TIMING
		++timing.waits;
#endif
		transmitList.insert(std::make_pair(seq, vector));
	}
	else {
		sendVector(vector);
		// now send any pending vectors
		while (!transmitList.empty()) {
			TransmitList::iterator p = transmitList.begin();
			if (p->first == curSeq) {
#if TRANSMITTER_TIMING
				++timing.wakeups;
#endif
				vector = p->second;
				transmitList.erase(p);
				sendVector(vector);
			}
			else
				break;
		}
	}
#if TRANSMITTER_TIMING
	uint64_t t2 = getticks();
	timing.setup += elapsed(t1, t0);
	timing.send += elapsed(t2, t1);
	timing.total += elapsed(t2, t0);
#endif
}

void
TransmitterTask::sendVector(ChannelPacketVector *vector)
{
	// send all the packets
	for (uint32_t i = 0; i < vector->size(); ++i) {
		ChannelPacket& p = (*vector)[i];
		Assert(p.getHeader().chan < vector->size());
		send(p);
	}
#if TRANSMITTER_TIMING
	uint64_t t0 = getticks();
#endif
	vectorList->free(vector);
	++curSeq;
#if TRANSMITTER_TIMING
	uint64_t t1 = getticks();
	timing.free += elapsed(t1, t0);
#endif
}

void
TransmitterTask::restart()
{
	while (!transmitList.empty()) {
		TransmitList::iterator p = transmitList.begin();
		ChannelPacketVector *vector = p->second;
		transmitList.erase(p);
		vectorList->free(vector);
	}
	curSeq = 0;
}

int32_t
TransmitterTask::getWaits()
{
	return (timing.waits);
}

/**
 * Send a data packet to a socket.
 *
 * Description:\n
 * 	Sends a set of multicast data packets.\n\n
 * Notes:\n
 * 	Assumes that the packets are completely constructed.
*/
Error
TransmitterTask::send(ChannelPacket& pkt)
{
	// compute the IP address of the port
	ChannelDataPacket *packet = static_cast<ChannelDataPacket *>
			(pkt.getPacket());
	int32_t chan = packet->hdr.chan;
//	recordOutputStats(chan, (ComplexInt16 *) packet->data.samples);
	IpAddress ipAddr;
	convertChanToIp(ipAddr, chan);
//	packet->hdr.chan = 0;
	return (connection->send(packet, pkt.getPacketSize(), ipAddr,
			base.port + chan));
}

void
TransmitterTask::convertChanToIp(IpAddress& ipAddr, int32_t chan)
{
	in_addr tmp;
	tmp.s_addr = htonl(ntohl(base.inaddr.s_addr) + chan);
	strcpy(ipAddr, inet_ntoa(tmp));
}

void
TransmitterTask::recordOutputStats(int32_t chan, const ComplexInt16 *s)
{
	ComplexFloat64 val = ComplexFloat64(s[0].real(), s[0].imag());
	float64_t power = norm(val);
	// compute global statistics
	++outputStats.samples;
	if (power < norm(outputStats.min))
		outputStats.min = val;
	if (norm(val) > norm(outputStats.max))
		outputStats.max = val;
	outputStats.sum += val;
	outputStats.sumSq += power;
	// compute channel statistics
	SampleStatistics& stats = channelStats[chan];
	++stats.samples;
	if (power < norm(stats.min))
		stats.min = val;
	if (norm(val) > norm(stats.max))
		stats.max = val;
	stats.sum += val;
	stats.sumSq += power;
}

}
