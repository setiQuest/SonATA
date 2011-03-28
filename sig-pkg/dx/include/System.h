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
// DX primary header file
//
#ifndef _SystemH
#define _SystemH

#include <assert.h>
#include <complex>
#include <netinet/in.h>
#include <string>
#include <sys/syscall.h>
#include <fftw3.h>
#include "DxTypes.h"
#include "Sonata.h"

using std::string;
using namespace sonata_lib;

namespace dx {

const char *const CODE_VERSION = "kes-1.0.4";

// defines controlling system configuration
#define ASSIGN_CPUS			(true)

// defines archive format used; set to true to use 4-bit microfloats, to false
// to use legacy 4-bit integers.
#define FLOAT4_ARCHIVE		(false)

// defines controlling timing code (debug/tuning only)
#define DX_TIMING			(false)
#define CHANNEL_TIMING		(true)
#define WORKER_TIMING		(true)
#define LOG_PACKET_TIMES	(false)

// defines controlling multicast group add code
#define DELAY_MULTICAST_ADD (true)

// macro to get thread id
#define gettid()			syscall(SYS_gettid)

// compute the maximum drift rate for pulse detection
#define COMPUTE_MAX_DRIFT (true)

/**
 * This is a hack, pure and simple.  It is used to adjust the baseline
 * power to allow for the fact that the subchannel baseline includes all
 * bins, including those in the transition band, which have lower average
 * power than the bins in the passband.  The baseline is increased by
 * this factor.  Note that changing filters and/or overlap will change
 * this value, so we should find a way to calculate it.
 */
//const float32_t BASELINE_FACTOR = 1.0;
const float32_t BASELINE_FACTOR = 1.098;
//const float32_t BASELINE_FACTOR = 1.0;

// constants
const int32_t BITS_PER_BYTE = 8;

// system values which may change during testing

const uint32_t BUF_COUNT = 8;

// system definitions
const IpAddress MULTICAST_ADDR = "227.1.1.1";
const int MULTICAST_PORT = 51000;
const int32_t DEFAULT_RCV_BUFSIZE = 4 * 1024 * 1024;
const int32_t POLARIZATIONS = 2;
const int32_t RESOLUTIONS = 11;
const int32_t HALF_FRAMES_PER_FRAME = 2;
const int32_t MAX_PACKET_ERROR = 1000;
const int32_t MAX_ACTIVITIES = 2;
const int32_t MAX_STR_LEN = 50;
const int32_t MAX_SUBCHANNELS = 8192;
const int32_t MAX_CD_VAL = 7;
const float64_t MIN_SNR_RATIO_ERROR = .3;
const float64_t MAX_SNR_RATIO_ERROR = 3.0;
const int32_t SUBCHANNEL_SAMPLES_PER_HALF_FRAME = 512;
const int32_t HALF_FRAME_BUFFERS = 4;
const int32_t TOTAL_BINS_PER_SUBCHANNEL_1HZ = 1024;
const int32_t MAX_FRAMES = 512;
const int32_t MAX_HALF_FRAMES = MAX_FRAMES * HALF_FRAMES_PER_FRAME + 1;
const int32_t MAX_SPECTRA_1HZ = 2 * MAX_FRAMES;
const int32_t MAX_SPECTRA_2HZ = 4 * MAX_FRAMES;
const int32_t MAX_SPECTRA_4HZ = 8 * MAX_FRAMES;
const int32_t MAX_SPECTRA_PER_FRAME_1HZ = 2;
const int32_t MAX_SPECTRA_PER_FRAME_2HZ = 4;
const int32_t MAX_SPECTRA_PER_FRAME_4HZ = 8;
const int32_t SUBCHANNEL_MASK_LEN = MAX_SUBCHANNELS;
const int32_t CWD_XLAT_SIZE = 256;
const int32_t DADD_BAND_BINS = 4096;
const int32_t ARCHIVE_BUFS = 4;
const int32_t CANDIDATE_BUFS = 4;
const int32_t ARCHIVE_SUBCHANNELS = 16;
const int32_t PULSE_SAFETY_FACTOR = 4;

const int32_t CD_BINS_PER_SUBCHANNEL = 1;
const float64_t CD_BYTES_PER_BIN = 1;
const int32_t CWD_BITS_PER_BIN = 2;
const int32_t CWD_BINS_PER_BYTE = 4;
const float64_t CWD_BYTES_PER_BIN = (1.0 / CWD_BINS_PER_BYTE);
const int32_t BL_BYTES_PER_BIN = sizeof(float);
const int32_t BL_SPECTRA_PER_HALF_FRAME = 1;
const int32_t BL_BINS_PER_SUBCHANNEL = 1;

// defaults
const int32_t DEFAULT_PORT = 8888;
const char DEFAULT_HOST[] = "gecko";
const int32_t DEFAULT_MAX_FRAMES = 64;
const int32_t DEFAULT_ID = 1;
const int32_t DEFAULT_SRC = 1;
const int32_t DEFAULT_MESSAGES = 1000;
const int32_t DEFAULT_QSLOTS = 50;
const int32_t DEFAULT_PACKETS = 1000;
const int32_t DEFAULT_SPECTRA_HALF_FRAMES = 3;
const int32_t DEFAULT_FREQ = 1420.0;
const float64_t DEFAULT_CHANNEL_WIDTH_MHZ = (104.8576 / 256);
const float64_t DEFAULT_CHANNEL_OVERSAMPLING = .25;
const float64_t DEFAULT_SUBCHANNELS = 1024;
const float64_t DEFAULT_SUBCHANNEL_OVERSAMPLING = .25;
const int32_t DEFAULT_SUBCHANNEL_FOLDINGS = 7;
const float64_t DEFAULT_BIN_OVERSAMPLING = .50;
const int32_t MAX_DEDRIFT_SAMPLES = 64;
const int32_t DEFAULT_MSGS = 1000;
const int32_t CHANNEL_PACKETS = 1000;
const int32_t INPUT_BUFFERS = 2;

const int32_t DADD_CACHE_ROWS = 16;
const float64_t FOLLOW_UP_FREQ_RANGE = 100e-6;

// partitions
const size_t PART1_SIZE = 64;
const int32_t PART1_BLKS = 2048;
const size_t PART2_SIZE = 1024;
const int32_t PART2_BLKS = 2048;
const size_t PART3_SIZE = 10000;
const int32_t PART3_BLKS = 1000;
const size_t PART4_SIZE = 30000;
const int32_t PART4_BLKS = 256;

// pulse detector
const uint32_t MAX_SUBCHANNEL_PULSES = 30;
const int32_t BINS_PER_PULSE_SLICE_1HZ = 4096;
const int32_t MIN_DELTA_SPECTRA = 2;
const int32_t MAX_DIFF_BINS = 1;
const int32_t MAX_DIFF_SPECTRA = 1;
const uint32_t MAX_TRAIN_PULSES = 250;
const float64_t MAX_DRIFT = 1.0;
//const int32_t BASELINE_ACT_DELAY_HF = 4;

const int32_t DEFAULT_CLUSTERS = 1000;

// maximum # of channels supported
//const int32_t MAX_CHANNELS = 1;

// CWD power bin statistics
const float64_t CWD_MEAN_BIN_POWER = 0.553;
const float64_t CWD_STDEV_BIN_POWER = 0.846;

const int32_t SSE_RETRY_SLEEP_TIME = 5 * MSEC_PER_SEC;
const int32_t ARCHIVER_RETRY_SLEEP_TIME = 5 * MSEC_PER_SEC;

// task priorities

const int32_t RECEIVER_PRIO = 22;
const int32_t INPUT_PRIO = 21;
const int32_t WORKER_PRIO = 20;
const int32_t COLLECTION_PRIO = 19;
const int32_t DETECTION_PRIO = 15;
const int32_t CONFIRMATION_PRIO = 15;
const int32_t CMD_PRIO = 10;
const int32_t CONTROL_PRIO = 10;
const int32_t OUTPUT_PRIO = 10;
const int32_t SSE_CONNECTION_PRIO = 10;
const int32_t ARCHIVER_CONNECTION_PRIO = 10;
const int32_t ARCHIVER_CMD_PRIO = 10;
const int32_t ARCHIVER_INPUT_PRIO = 10;
const int32_t ARCHIVER_OUTPUT_PRIO = 10;
const int32_t SSE_INPUT_PRIO = 10;
const int32_t ARCHIVE_PRIO = 10;
const int32_t CWD_PRIO = 9;
const int32_t PULSE_PRIO = 9;
const int32_t CWD_CONFIRMATION_PRIO = 9;
const int32_t PULSE_CONFIRMATION_PRIO = 9;

}

#endif
