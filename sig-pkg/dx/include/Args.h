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
// Command-line arguments class; singleton
//
// $Header: /home/cvs/nss/sonata-pkg/dx/include/Args.h,v 1.10 2009/06/16 15:13:11 kes Exp $
//
#ifndef _ArgsH
#define _ArgsH

#include <stdlib.h>
#include <string>
#include <ssePorts.h>
#include "Err.h"
#include "ReadFilter.h"
#include "System.h"

using std::string;
using namespace sonata_lib;

namespace dx {

class Args {
public:
	static Args *getInstance(int argc, char **argv);
	static Args *getInstance();

	~Args();

	bool singlePol() {
		return (flags.onePol);
	}
	Polarization getPol() {
		return (polarization);
	}
	bool useTaggedData() {
		return (flags.taggedData);
	}
	bool swapInputs() {
		return (flags.swap);
	}
	bool doCacheEfficientDadd() {
		return (dadd.cacheEfficient);
	}
	bool initBuffers() {
		return (flags.initBufs);
	}
	bool runTest() {
		return (flags.test);
	}
	bool loadSliceNum() {
		return (flags.loadSlice);
	}
	bool loadSubchannelNum() {
		return (flags.loadSubchannel);
	}
	bool logSseMessages() {
		return (flags.logSseMsgs);
	}
	bool useHanning() {
		return (flags.hanning);
	}
	bool reportCwStats() {
		return (flags.cwStats);
	}
	bool reportHits() {
		return (flags.logHits);
	}
	bool suppressBaselineReport() {
		return (flags.suppressBaselines);
	}
	bool zeroDCBins() {
		return (flags.zeroDCBin);
	}
	bool zxMode() {
		return (flags.zxMode);
	}
	int32_t getSubchannels() {
		return (subchannels);
	}
	int32_t getCacheRows() {
		return (dadd.cacheRows);
	}
	string getName() {
		return (name);
	}
	string getHost() {
		return (host);
	}
	int32_t getPort() {
		if (port)
			return (port);
		return (atoi(DEFAULT_DX_PORT));
	}
	string getMcAddr() {
		return (mcAddr);
	}
	int32_t getMcPort() {
		return (mcPort);
	}
	int32_t getMaxFrames() {
		return (maxFrames);
	}
	int32_t getFoldings() {
		return (foldings);
	}
	float64_t getOversampling() {
		return (oversampling);
	}
	float64_t getChannelOversampling() {
		return (chanOversampling);
	}
	float64_t getChannelBandwidth() {
		return (chanBandwidth);
	}
	uint32_t getSrc() {
		return (src);
	}
	const string& getFilterFile() {
		return (filterFile);
	}
	bool useCustomFilter() {
		return (flags.customFilter);
	}
	const FilterSpec& getFilter() {
		return (filter);
	}
	const int32_t getRequestedSubchannels() {
		return (requestedSubchannels);
	}

private:
	static Args *instance;

	struct Flags {
		bool onePol;					// run a single polarization (clone it)
		bool taggedData;				// use tagged data
		bool swap;						// swap input (real and imaginary)
		bool initBufs;					// initialize buffers
		bool logSseMsgs;				// log messages from SSE
		bool hanning;					// use Hanning window for CW
		bool cwStats;					// report CW stats
		bool logHits;					// log hits to SSE
		bool loadSlice;					// store slice number in CWD data
		bool loadSubchannel;			// store subchannel number in CD data
		bool suppressBaselines;			// suppress baseline reports
		bool test;						// run timing test on startup
		bool zeroDCBin;					// zero the DC bins in CWD
		bool zxMode;					// run as ZX rather than DX
		bool customFilter;				// use a custom filter file

		Flags(): onePol(false), taggedData(false),
				swap(false), initBufs(false),
				logSseMsgs(false), hanning(true), cwStats(true),
				logHits(true), loadSlice(false), loadSubchannel(false),
				suppressBaselines(false), test(false), zeroDCBin(false),
				zxMode(false), customFilter(false)
				{}
	} flags;
	struct Dadd {
		bool cacheEfficient;			// use divide and conquer DADD
		int32_t cacheRows;				// # of DADD rows to cache

		Dadd(): cacheEfficient(true), cacheRows(DADD_CACHE_ROWS) {}
	} dadd;

	string name;						// name of this detector
	string host;						// host name
	string mcAddr;						// multicast base address
	string filterFile;					// filter file
	int32_t port;						// host port number
	int32_t mcPort;						// multicast port
	uint32_t src;						// source
	int32_t subchannels;				// total # of subchannels
	int32_t usableSubchannels;			// # of usable subchannels
	int32_t maxFrames;					// max # of frames in an activity
	int32_t foldings;					// # of foldings in DFB filter
	int32_t requestedSubchannels;		// # of requested subchannels
	float64_t oversampling;				// percentage of oversampling
	float64_t chanOversampling;			// percentage of channel oversampling
	float64_t chanBandwidth;			// nominal channel bandwidth
	Polarization polarization;			// polarization
	FilterSpec filter;					// custom filter specification

	void parse(int argc, char **argv);
	void usage();

	// hidden
	Args(int argc, char **argv);

	// forbidden
	Args(const Args&);
	Args& operator=(const Args&);
};

}

#endif
