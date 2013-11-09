/*******************************************************************************

 File:    Args.h
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
// Command-line arguments class
//
// $Header: /home/cvs/nss/sonata-pkg/channelizer/include/Args.h,v 1.12 2009/05/24 22:16:04 kes Exp $
//
#ifndef _ArgsH
#define _ArgsH

#include <string>
#include <ssePorts.h>
#include "BeamPacket.h"
#include "ChStruct.h"
#include "Err.h"
#include "ReadFilter.h"
#include "System.h"
#include "ChVersion.h"

using namespace sonata_lib;
using std::string;

namespace chan {

class Args {
public:
	static Args *getInstance();

	void showDefaults();
	~Args();

	Error parse(int argc, char **argv);

	bool initBuffers() { return (initBufs); }
	bool noSse() { return (localMode); }
	bool logSseMessages() { return (logSseMsgs); }
	bool useCustomFilter() { return (customFilter); }
	bool sendStats() { return (multicastStats); }
	bool printStats() { return (printStatistics); }
	bool printChannelStats() { return (printChannelStatistics); }
	bool swapInputs() { return (swap); }
	const string& getName() { return (chName); }
	const FilterSpec& getFilter() { return (filter); }
	const string& getFilterFile() { return (filterFile); }
	int32_t getTotalChannels() { return (channels.total); }
	int32_t getUsableChannels() { return (channels.usable); }
	int32_t getFoldings() { return (channels.foldings); }
	uint32_t getBeamSrc() { return (beamSrc); }
	uint32_t getChannelSrc() { return (channelSrc); }
	uint32_t getStartTime() { return (startTime); }
	::uint8_t getPol() { return (pol); }
	float64_t getCenterFreq() { return (centerFreq); }
	float64_t getBandwidth() { return (bandwidth); }
	float64_t getOversampling() { return (oversampling); }
	const ChannelSpec& getChannels() { return (channels); }
	const HostSpec& getSse() { return (sse); }
	const HostSpec& getInput() { return (input); }
	const HostSpec& getOutput() { return (output); }
	const HostSpec& getStatistics() { return (statistics); }
	string getCfgFile() { return (cfgFile); }
	IpAddress& getSseAddr() { return (sse.addr); }
	int32_t getSsePort() { return (sse.port); }
	IpAddress& getInputAddr() { return (input.addr); }
	int32_t getInputPort() { return (input.port); }
	IpAddress& getOutputAddr() { return (output.addr); }
	int32_t getOutputPort() { return (output.port); }
	int32_t getReceiverCpu() { return (receiverCpu); }
	IpAddress& getStatisticsAddr() { return (statistics.addr); }
	int32_t getStatisticsPort() { return (statistics.port); }
	int32_t getWorkers() { return (workers); }
	int32_t getDecimation() { return (decimation); }
	//JR - Nov 05, 2013
	uint32_t getWorkersFirstCPU() { return (workersFirstCPU); }
	uint32_t getTransmitterCPU() { return (transmitterCPU); }

private:
	static Args *instance;

	bool initBufs;						// initialize buffers
	bool localMode;						// local mode (no SSE)
	bool logSseMsgs;					// log messages from SSE
	bool customFilter;					// use a custom filter
	bool multicastStats;				// send statistics to multicast
	bool printStatistics;				// print general statistics
	bool printChannelStatistics;		// print output channel statistics
	bool swap;							// swap real and imaginary inputs
	::uint8_t pol;						// polarization
	int32_t decimation;					// decimation of samples
	int32_t receiverCpu;				// CPU to assign receiver task to
	int32_t workers;					// # of worker tasks
	uint32_t beamSrc;					// beam source (input packets)
	uint32_t channelSrc;				// channel source (output packets)
	uint32_t startTime;					// starting time for data collection
	float64_t centerFreq;				// center frequency
	float64_t bandwidth;				// input bandwidth
	float64_t oversampling;				// % of oversampling
	string cfgFile;						// input configuration file
	string chName;						// channelizer name
	string filterFile;					// filter specification file
	ChannelSpec channels;				// channel information
	FilterSpec filter;					// DFB filter specification
	HostSpec sse;						// SSE host specification
	HostSpec input;						// input host specification
	HostSpec output;					//  output address specification
	HostSpec statistics;				//  statistics address specification
	//JR - Nov 05, 2013 - Added to allow specifying which CPU for the worker threads
	uint32_t workersFirstCPU;                        //The first CPU for the worker threads.
	uint32_t transmitterCPU;                        //The CPU to run the Transmitter worker thread.

	// hidden
	Args();

	// forbidden
	Args(const Args&);
	Args& operator=(const Args&);
};

}

#endif
