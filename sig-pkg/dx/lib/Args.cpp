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
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/Args.cpp,v 1.11 2009/06/16 15:13:56 kes Exp $
//
#include <iostream>
#include <fcntl.h>
#include "System.h"
#include "Args.h"
#include "Lock.h"

using std::cout;
using std::endl;
using namespace sonata_lib;

namespace dx {

static string usageString = "sudo dx [-H host] [-h port] [-r] [-j dadd_device ] [-(s|z|g)] [-n noise] [-t] [-x ip.file] [ -o output_dir] [-b board number] -f [ddc bit file; - to skip download] [-w port]\n\
	-c cacheRows: use cacheRows for cache efficient DADD\n\
	-D: zero DC bin\n\
	-E: suppress reporting of baselines \n\
	-e: run timing tests at startup\n\
	-F frames: set maximum number of frames\n\
	-f foldings: number of subchannel DFB foldings\n\
	-H host: connect directly to host\n\
	-h port: connect to host port\n\
	-I: initialize buffers during allocation\n\
	-J addr: multicast base address\n\
	-j port: multicast port\n\
	-k oversampling: subchannel DFB oversampling\n\
	-L: log hits\n\
	-l: linear polarization\n\
	-M: log all messages from SSE\n\
	-m: don't use Hanning window for CW\n\
	-N: load subchannel number into CD data\n\
	-n subchannels: max # of requested subchannels\n\
	-O sigma: suppress DADD saturation at less than sigma\n\
	-p [C|L|x|y|l|r]: polarization Circular, Linear, x, y, left or right\n\
	-Q name: name of this detector\n\
	-R: report CWD bin statistics\n\
	-r: use tagged data\n\
	-T subchannels: total # of subchannels to create\n\
	-t: run top-down DADD\n\
	-u: swap real and imaginary\n\
	-U oversampling: oversampling percentage for subchannels\n\
	-V: load slice number into CWD data\n\
	-v val: insert val (0-3) for each bin\n\
	-W oversampling: oversampling percentage for channels \n\
	-w bandwidth: nominal channel bandwidth\n\
	-X file: file that contains standalone observation parameters \n\
	-x src: source\n\
	-z file: read filter specification from file\n\
	-Z: run as ZX\n\\n";

Args *Args::instance = 0;

Args *
Args::getInstance(int argc,  char **argv)
{
	static Lock l;
	l.lock();
	if (!instance)
		instance = new Args(argc, argv);
	l.unlock();
	return (instance);
}

Args *
Args::getInstance()
{
	Assert(instance);
	return (instance);
}

Args::Args(int argc, char **argv): name(""), host(DEFAULT_HOST),
		mcAddr(MULTICAST_ADDR),
		filterFile(""), port(DEFAULT_PORT), mcPort(MULTICAST_PORT),
		src(DEFAULT_SRC), subchannels(DEFAULT_SUBCHANNELS),
		usableSubchannels(DEFAULT_SUBCHANNELS), maxFrames(DEFAULT_MAX_FRAMES),
		foldings(DEFAULT_SUBCHANNEL_FOLDINGS),
		requestedSubchannels(1),
		oversampling(DEFAULT_SUBCHANNEL_OVERSAMPLING),
		chanOversampling(DEFAULT_CHANNEL_OVERSAMPLING),
		chanBandwidth(DEFAULT_CHANNEL_WIDTH_MHZ), polarization(POL_BOTHLINEAR)
{
	parse(argc, argv);
}

Args::~Args()
{
}

// grant access to external variables
namespace std {
	extern char *optarg;
	extern int optind, optopt, opterr;
}

void
Args::parse(int argc, char **argv)
{
	bool done = false;
	const char *optstring = "DeEILMmNRstuVZ1:B:C:c:d:f:F:H:h:i:J:j:n:o:P:p:Q:S:T:U:v:W:w:X:x:Y:y:z:";

	opterr= 0;
	while (!done) {
		switch (getopt(argc, argv, optstring)) {
		case -1:
			done = true;
			break;
		case 'c':
			dadd.cacheRows = atoi(optarg);
			dadd.cacheEfficient = true;
			break;
		case 'D':
			flags.zeroDCBin = true;
			break;
		case 'e':
			flags.test = true;
			break;
		case 'E':
			flags.suppressBaselines = true;
			break;
		case 'f':
			foldings = atoi(optarg);
			break;
		case 'F':
			maxFrames = atoi(optarg);
			break;
		case 'H':
			host = string(optarg);
			break;
		case 'h':
			port = atoi(optarg);
			break;
		case 'I':
			flags.initBufs = true;
			break;
		case 'j':
			mcPort = atoi(optarg);
			break;
		case 'J':
			mcAddr = string(optarg);
			break;
		case 'L':
			flags.logHits = true;
			break;
		case 'M':
			flags.logSseMsgs = true;
			break;
		case 'm':
			flags.hanning = false;
			break;
		case 'N':
			flags.loadSubchannel = true;
			break;
		case 'n':
			requestedSubchannels = atoi(optarg);
			break;
		case 'p':
			{
			int32_t c = optarg[0];
			switch (c) {
			case 'C':
				polarization = POL_BOTH;
				break;
			case 'L':
				polarization = POL_BOTHLINEAR;
				break;
			case 'l':
				flags.onePol = true;
				polarization = POL_LEFTCIRCULAR;
				break;
			case 'r':
				flags.onePol = true;
				polarization = POL_RIGHTCIRCULAR;
				break;
			case 'x':
				flags.onePol = true;
				polarization = POL_XLINEAR;
				break;
			case 'y':
				flags.onePol = true;
				polarization = POL_YLINEAR;
				break;
			default:
				usage();
			}
			}
			break;
		case 'Q':
			name = string(optarg);
			break;
		case 'R':
			flags.cwStats = true;
			break;
		case 'r':
			flags.taggedData = true;
			break;
		case 'T':
			subchannels = atoi(optarg);
			break;
		case 't':
			dadd.cacheEfficient = false;
			break;
		case 'u':
			flags.swap = true;
			break;
		case 'U':
			oversampling = atof(optarg);
			break;
		case 'V':
			flags.loadSlice = true;
			break;
		case 'W':
			chanOversampling = atof(optarg);
			break;
		case 'w':
			chanBandwidth = atof(optarg);
			break;
		case 'x':
			src = atoi(optarg);
			break;
		case 'z':
			filterFile = string(optarg);
			break;
		case 'Z':
			flags.zxMode = true;
			break;
		default:
			cout << "Invalid option: " << optopt << endl;
			usage();
		}
	}
	if (filterFile.length()) {
		ReadFilter read;
		if (read.readFilter(filterFile, filter)) {
			cout << "invalid filter file" << endl;
			exit(1);
		}
		flags.customFilter = true;
	}
}

void
Args::usage()
{
	cout << usageString;
	exit(-1);
}

}
