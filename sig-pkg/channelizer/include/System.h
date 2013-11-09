/*******************************************************************************

 File:    System.h
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
// Channelizer primary header file
//
// $Header: /home/cvs/nss/sonata-pkg/channelizer/include/System.h,v 1.23 2009/05/24 22:19:01 kes Exp $
//
#ifndef _SystemH
#define _SystemH

#include <assert.h>
#include <complex>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/syscall.h>
#if defined(__x86_64__) || defined(__SSE2__)
#include <xmmintrin.h>
#endif
#include <fftw3.h>
#include "BeamPacket.h"
#include "ChTypes.h"
#include "Sonata.h"

using namespace sonata_lib;

namespace chan {

#if defined(__x86_64__) || defined(__SSE2__)
// vector operators
typedef int8_t v16b __attribute__ ((vector_size(16)));
typedef int16_t v8s __attribute__ ((vector_size(16)));
typedef int32_t v4w __attribute__ ((vector_size(16)));
typedef float32_t v4sf __attribute__ ((vector_size(16)));
#endif

// defines controlling system configuration
#define ASSIGN_CPUS				(true)

// defines controlling timing code (debug only)
#define BEAM_TIMING				(true)
#define CHANNELIZER_TIMING		(false)
#define INPUT_TIMING			(false)
#define RECEIVER_TIMING			(false)
#define OUTPUT_TIMING			(false)
#define TRANSMITTER_TIMING		(false)
#define CHANNEL_PACKET_VECTOR_TIMING (false)
#define WORKER_TIMING			(false)

// macro to get thread id
#define gettid()		syscall(SYS_gettid)

// for input to the Channelizer
const uint32_t BUF_COUNT = 32768;
const int32_t MAX_TOTAL_CHANNELS = 1024;
const int32_t DEFAULT_TOTAL_CHANNELS = 256;
const int32_t DEFAULT_USABLE_CHANNELS = 230;
const int32_t DEFAULT_VECTORS = 1024;
const int32_t DEFAULT_OVERLAP = 52;
const int32_t DEFAULT_FOLDINGS = 7;
const int32_t DEFAULT_SAMPLES_PER_PACKET = 1024;
const float64_t DEFAULT_CENTER_FREQ = 1420.;
const float64_t DEFAULT_BANDWIDTH = 104.8576;
const float64_t DEFAULT_OVERSAMPLING = .203125;

const int32_t DEFAULT_RCV_BUFSIZE = 16 * 1024 * 1024;
const int32_t DEFAULT_SND_BUFSIZE = 16 * 1024 * 1024;

// system defaults
const string DEFAULT_CFG_FILE = "channelizer.cfg";
const string DEFAULT_NAME = "chan1x";
const IpAddress DEFAULT_SSE_ADDR = "sse-main";
const int32_t DEFAULT_SSE_PORT = 8870;
const IpAddress DEFAULT_INPUT_ADDR = "226.1.50.1";
const int32_t DEFAULT_INPUT_PORT = 50000;
const IpAddress DEFAULT_OUTPUT_ADDR = "227.1.1.1";
const int32_t DEFAULT_OUTPUT_PORT = 51000;
const IpAddress DEFAULT_STATISTICS_ADDR = "225.1.1.1";
const int32_t DEFAULT_STATISTICS_PORT = 55000;
const uint32_t DEFAULT_BEAMSRC = ATADataPacketHeader::BEAM_104MHZ;
const uint32_t DEFAULT_CHANSRC = ATADataPacketHeader::CHAN_400KHZ;
const uint32_t DEFAULT_START_TIME = 0;
const int32_t DEFAULT_WORKERS = 1;
const int32_t DEFAULT_WORKERS_STARTCPU = 3;
const int32_t DEFAULT_TRANSMITTER_CPU = 2;
const int32_t DEFAULT_RECEIVERS = 1;
const int32_t RECEIVER_DELAY = 500;
const int32_t CHANNELIZER_MESSAGES = 100000;
const int32_t DEFAULT_WORKQ_SLOTS = 100000;
const int32_t DEFAULT_INPUTQ_SLOTS = 100000;
const int32_t DEFAULT_TRANSMITQ_SLOTS = 100000;

// system definitions
const size_t PART1_SIZE = 256;
const int32_t PART1_BLKS = 100000;
const size_t PART2_SIZE = 1024;
const int32_t PART2_BLKS = 100000;
const size_t PART3_SIZE = 5000;
const int32_t PART3_BLKS = 100000;

const int32_t SSE_RETRY_SLEEP_TIME = 5 * MSEC_PER_SEC;

#ifdef notdef
#ifndef NTOHL
#define NTOHS(x)						((x) = ntohs(x))
#define NTOHL(x)						((x) = ntohl(x))
#endif
#endif

// task priorities
const int32_t RECEIVER_PRIO = 23;
const int32_t TRANSMITTER_PRIO = 24;
const int32_t INPUT_PRIO = 21;
const int32_t WORKER_PRIO = 19;
const int32_t CMD_PRIO = 18;
const int32_t SSE_CONNECTION_PRIO = 10;
const int32_t SSE_INPUT_PRIO = 10;
const int32_t SSE_OUTPUT_PRIO = 10;
const int32_t LOG_PRIO = 1;

}

#endif
