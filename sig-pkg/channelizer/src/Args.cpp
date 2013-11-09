/*******************************************************************************

 File:    Args.cpp
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
// Command-line arguments class (singleton)
//
//
#include <iostream>
#include <fcntl.h>
#include "System.h"
#include "Args.h"
#include "Lock.h"
#include "ChVersion.h"

using std::cout;
using std::endl;
using namespace sonata_lib;

namespace chan {

static string usage = "channelizer [-?] [-A addr] [-a port] [-b] [-B bw] [-c channels] [-C channels] [-D decimation] [-d filter file] [-f file] [-h port] [-H host] [-i port] [-I host] [-j port] [-J host] [-M] [-m] [-N foldings] [-O oversampling] [-o] [-P polarization] [-S src] [-w workers] [-W worker thread first CPU]\n\
	-?: print usage and exit\n\
	-A addr: statistics address\n\
	-a port: statistics port\n\
	-b: initialize buffers during allocation\n\
	-B bw: bandwidth bw (MHz)\n\
	-C channels: total channels\n\
	-c channels: usable channels\n\
	-D decimation: # of input samples to add to make buffer sample\n\
	-d file: read filter specification from file\n\
	-F freq: center frequency freq (MHz)\n\
	-f file: read parameters from configuration file\n\
	-H addr: sse name or ip address\n\
	-h port: sse port \n\
	-I addr: use input address addr\n\
	-i port: use input port port\n\
	-J addr: use output address addr\n\
	-j port: use output port port\n\
	-L: local mode (no SSE)\n\
	-M: multicast statistics packets\n\
	-m: log all messages from SSE\n\
	-N foldings: # of foldings in filter bank\n\
	-n: don't swap real and imaginary input values\n\
	-O oversampling: % of oversampling\n\
	-o: print output channel statistics\n\
	-P polarization: polarization (X or Y)\n\
	-p: print general output statistics\n\
	-Q chname: channelizer name\n\
	-R receiver: cpu to use for receiver task. Zero based.\n\
	-S src: beam source (uint32, input packets)\n\
	-s src: channel source (uint32_t, output packets)\n\
	-t time: start time in unix time (sec)\n\
	-T cpu: CPU to run transmitter thread. Zero based.\n\
	-V: print version and exit\n\
	-w workers: number of worker tasks\n\
	-W start CPU core: first CPU core for worker threads (3)\n\\n\\n";

Args *Args::instance = 0;

Args *
Args::getInstance()
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new Args();
	l.unlock();
	return (instance);
}

Args::Args(): initBufs(false), localMode(false), logSseMsgs(false),
		customFilter(false), multicastStats(false), printStatistics(false),
		printChannelStatistics(false), swap(true),
		pol(ATADataPacketHeader::XLINEAR),
		decimation(1), receiverCpu(-1), workers(DEFAULT_WORKERS),
		beamSrc(DEFAULT_BEAMSRC),
		channelSrc(DEFAULT_CHANSRC), startTime(DEFAULT_START_TIME),
		centerFreq(DEFAULT_CENTER_FREQ), bandwidth(DEFAULT_BANDWIDTH),
		oversampling(DEFAULT_OVERSAMPLING), cfgFile(DEFAULT_CFG_FILE),
		chName(DEFAULT_NAME),
		filterFile(""), channels(), sse(DEFAULT_SSE_ADDR, DEFAULT_SSE_PORT),
		input(DEFAULT_INPUT_ADDR, DEFAULT_INPUT_PORT),
		output(DEFAULT_OUTPUT_ADDR, DEFAULT_OUTPUT_PORT),
		statistics(DEFAULT_STATISTICS_ADDR, DEFAULT_STATISTICS_PORT),
		workersFirstCPU(DEFAULT_WORKERS_STARTCPU), transmitterCPU(DEFAULT_TRANSMITTER_CPU)

{
}

Args::~Args()
{
}

// grant access to external variables
namespace std {
	extern char *optarg;
	extern int optind, optopt, opterr;
}

Error
Args::parse(int argc, char **argv)
{
	bool done = false;
	const char *optstring = "?bLMmnopVA:a:B:C:c:D:d:F:f:H:h:I:i:J:j:l:N:O:P:Q:R:S:s:t:T:w:W:";

	opterr= 0;
	while (!done) {
		switch (getopt(argc, argv, optstring)) {
		case -1:
			done = true;
			break;
		case '?':
			showDefaults();
			exit(1);
		case 'A':
			memcpy(statistics.addr, optarg, sizeof(statistics.addr));
			break;
		case 'a':
			statistics.port = atoi(optarg);
			break;
		case 'B':
			bandwidth = atof(optarg);
			break;
		case 'b':
			initBufs = true;
			break;
		case 'C':
			channels.total = atoi(optarg);
			break;
		case 'c':
			channels.usable = atoi(optarg);
			break;
		case 'D':
			decimation = atoi(optarg);
			break;
		case 'd':
			filterFile = string(optarg);
			break;
		case 'F':
			centerFreq = atof(optarg);
			break;
		case 'f':
			cfgFile = string(optarg);
			break;
		case 'H':
			memcpy(sse.addr, optarg, sizeof(sse.addr));
			break;
		case 'h':
			sse.port = atoi(optarg);
			break;
		case 'I':
			memcpy(input.addr, optarg, sizeof(input.addr));
			break;
		case 'i':
			input.port = atoi(optarg);
			break;
		case 'J':
			memcpy(output.addr, optarg, sizeof(output.addr));
			break;
		case 'j':
			output.port = atoi(optarg);
			break;
		case 'L':
			localMode = true;
			break;
		case 'M':
			multicastStats = true;
			break;
		case 'm':
			logSseMsgs = true;
			break;
		case 'N':
			channels.foldings = atoi(optarg);
			break;
		case 'n':
			swap = false;
			break;
		case 'O':
			oversampling = atof(optarg);
			break;
		case 'o':
			printStatistics = true;
			printChannelStatistics = true;
			break;
		case 'P':
			switch (*optarg) {
			case 'R':
			case 'r':
				pol = ATADataPacketHeader::RCIRC;
				break;
			case 'L':
			case 'l':
				pol = ATADataPacketHeader::LCIRC;
				break;
			case 'X':
			case 'x':
				pol = ATADataPacketHeader::XLINEAR;
				break;
			case 'Y':
			case 'y':
				pol = ATADataPacketHeader::YLINEAR;
				break;
			default:
				cout << usage;
				exit(1);
			}
			break;
		case 'p':
			printStatistics = true;
			break;
		case 'Q':
			chName = string(optarg);
			break;
		case 'R':
			receiverCpu = atoi(optarg);
			break;
		case 'S':
			beamSrc = atoi(optarg);
			break;
		case 's':
			channelSrc = atoi(optarg);
			break;
		case 't':
			startTime = atoi(optarg);
			break;
		case 'T':
			transmitterCPU = atoi(optarg);
			break;
		case 'V':
			cout << CHVERSION << endl;
			exit(1);
		case 'w':
			workers = atoi(optarg);
			break;
		case 'W':
			workersFirstCPU = atoi(optarg);
			break;
		default:
			cout << "Invalid option: " << optopt << endl;
			//cout << usage;
                        showDefaults();
			exit(-1);
		}
	}
	// load the filter coefficients if necessary
	if (filterFile.length()) {
		ReadFilter read;
		if (read.readFilter(filterFile, filter)) {
			cout << "Invalid filter file" << endl;
			exit(1);
		}
		customFilter = true;
	}
	// do limited validity test
	if (channels.total < channels.usable) {
		cout << "Total channels must be >= usable channels" << endl;
		exit(1);
	}
	int32_t samples = (int32_t) (channels.total * oversampling);
#ifdef notdef
	if ((samples & 1) || (samples != channels.total * oversampling)) {
#else
	if (samples != channels.total * oversampling) {
#endif
		cout << "Overlap samples (total channels * oversampling) must be integer";
		cout << endl;
		exit(1);
	}
	return (0);
}
void Args::showDefaults()
{

     cout << "channelizer [-?] [-b] [-B bw] [-c channels] [-C channels] [-D decimation] [-d filter file] [-f file] [-h port] [-H host] [-i port] [-I host] [-j port] [-J host] [-m] [-N foldings] [-O oversampling] [-o] [-P polarization] [-S src] [-w workers] [-T <transmitter CPU>]\n\
	-?: print usage and exit\n\
	-b: initialize buffers during allocation (false)\n\
	-B bw: bandwidth bw (MHz) (" << DEFAULT_BANDWIDTH << ")\n\
	-C channels: total channels (" << DEFAULT_TOTAL_CHANNELS << ")\n\
	-c channels: usable channels (" << DEFAULT_USABLE_CHANNELS << ")\n\
	-D decimation: # of input samples to add to make buffer sample (1)\n\
	-d file: read filter specification from file (NULL)\n\
	-F freq: center frequency freq (MHz)\n\
	-f file: read parameters from configuration file (" <<
		DEFAULT_CFG_FILE << ")\n\
	-H addr: sse name or ip address (" << DEFAULT_SSE_ADDR << ")\n\
	-h port: sse port (" << DEFAULT_SSE_PORT <<")\n\
	-I addr: use input address addr (" << DEFAULT_INPUT_ADDR <<")\n\
	-i port: use input port port (" << DEFAULT_INPUT_PORT <<")\n\
	-J addr: use output address addr (" << DEFAULT_OUTPUT_ADDR <<")\n\
	-j port: use output port port (" << DEFAULT_OUTPUT_PORT <<")\n\
	-L: local mode, no SSE\n\
	-m: log all messages from SSE (false)\n\
	-N foldings: # of foldings in filter bank (" << DEFAULT_FOLDINGS << ")\n\
	-n: don't swap real and imaginary input values (false)\n\
	-O oversampling: % of oversampling (" << DEFAULT_OVERSAMPLING << ")\n\
	-o: print output channel statistics (false)\n\
	-P polarization: polarization (X or Y) (" << ATADataPacketHeader::XLINEAR 
			<< ")\n\
	-Q chName: name of this channelizer (" << DEFAULT_NAME << ")\n\
	-R receiver thread CPU: (0)\n\
	-S src: beam source (uint32, input packets) (" << DEFAULT_BEAMSRC << ")\n\
	-s src: channel source (uint32_t, output packets) (" << DEFAULT_CHANSRC << ")\n\
	-T transmitting thread CPU: (" << DEFAULT_TRANSMITTER_CPU << ")\n\
	-V: print version and exit\n\
	-w workers: number of worker tasks (" << DEFAULT_WORKERS << ")\n\
	-W workers first CPU: first CPU core for worker threads (" << DEFAULT_WORKERS_STARTCPU << ")\n\\n\\n";
}

}
