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
// $Header: /home/cvs/nss/sonata-pkg/utilities/packetsend/src/Args.cpp,v 1.9 2009/05/25 00:21:05 kes Exp $
//
#include <iostream>
#include <fcntl.h>
#include "System.h"
#include "Args.h"
#include "BeamPacket.h"
#include "Lock.h"
#include "PsVersion.h"

using std::cout;
using std::endl;
using namespace sonata_lib;

namespace sonata_packetsend {

static string usage = "packetsend [-?] [-b burst] [-c][-f file] [-i delay] [-J host] [-j port] [-P polarization] [-r loopcnt] [-S src] [-n nchan]\n\
	-?: print usage and exit\n\
	-b burst: send burst length\n\
	-c: channel packets, not beam packets\n\
	-f file: read input data from file\n\
	-i delay: delay between bursts (usec)\n\
	-J addr: use output address addr\n\
	-j port: use output port port\n\
	-n nchan: Number of channels to send\n\
	-P polarization: polarization (X or Y)\n\
	-r loopcnt: number of times to loop through last buffers\n\
	-s: reset the header sequence numbers\n\
	-S src: beam source (int32)\n\
	-V: print version and exit\n\\n\\n";

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

Args::Args(): channels(false), reseq(false), resetPol(false), loop(0), 
	nchan(DEFAULT_NCHAN), burst(DEFAULT_BURST), delay(DEFAULT_DELAY), 
		pol(ATADataPacketHeader::XLINEAR), src(DEFAULT_SRC),
		inputFile(DEFAULT_INPUT_FILE),
		output(DEFAULT_OUTPUT_ADDR, DEFAULT_OUTPUT_PORT)



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
	const char *optstring = "?csVb:f:i:J:j:n:P:r:S:";

	opterr= 0;
	while (!done) {
		switch (getopt(argc, argv, optstring)) {
		case -1:
			done = true;
			break;
		case '?':
			cout << usage;;
			exit(1);
		case 'b':
			burst = atoi(optarg);
			break;
		case 'c':
			channels = true;
			src = ATADataPacketHeader::CHAN_400KHZ;
			memcpy(output.addr, CHANNEL_OUTPUT_ADDR, sizeof(output.addr));
			output.port = CHANNEL_OUTPUT_PORT;
			break;
		case 'f':
			inputFile = string(optarg);
			break;
		case 'i':
			delay = atoi(optarg);
			break;
		case 'J':
			memcpy(output.addr, optarg, sizeof(output.addr));
			break;
		case 'j':
			output.port = atoi(optarg);
			break;
		case 'n':
			nchan = atoi(optarg);
			break;
		case 'P':
			switch (*optarg) {
			case 'X':
			case 'x':
				resetPol = true;
				pol = ATADataPacketHeader::XLINEAR;
				break;
			case 'Y':
			case 'y':
				resetPol = true;
				pol = ATADataPacketHeader::YLINEAR;
				break;
			default:
				cout << usage;
				exit(1);
			}
			break;
		case 'r':
			loop = atoi(optarg);
			break;
		case 's':
			reseq = true;
			break;
		case 'S':
			src = atoi(optarg);
			break;
		case 'V':
			cout << PSVERSION << endl;
			exit(1);
		default:
			cout << "Invalid option: " << optopt << endl;
			cout << usage;;
			exit(-1);
		}
	}
	return (0);
}

}
