/*******************************************************************************

 File:    Spectrometer.h
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
// Spectrometer class
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/Spectrometer.h,v 1.8 2009/03/06 22:17:10 kes Exp $
//
#ifndef _SpectrometryTaskH
#define _SpectrometryTaskH

#include <sys/time.h>
#include <fftw3.h>
#include <SseOutputTask.h>
#include "System.h"
#include "Activity.h"
#include "Args.h"
#include "Buffer.h"
#include "Condition.h"
#include "DxStruct.h"
#include "Msg.h"
#include "Partition.h"
#include "QTask.h"
#include "Spectra.h"
#include "State.h"

using namespace sonata_lib;
using spectra::ResData;
using spectra::ResInfo;

namespace dx {

#ifdef notdef
// constants
const int32_t MAX_CD_VAL = 0x7ffffff8;
const int32_t MAX_CW_POWER = 0x7ffffffc;
const int32_t CW_BITS_PER_BIN = 2;
const int32_t MAX_PULSES_PER_FRAME = 5000;
const int32_t MAX_PULSES_PER_SUBCHANNEL = 10;
#endif

typedef std::deque<BufPair *> HfBufList;

/**
* Scalar structure for multiplying complex values by a constant.
*
* An array of these structures contains the FB filter coefficients
* for vector operations. \n
* Initialized by setup().
*
* Notes:
*	Re and im will always be the same.
*/
struct scalar {
	float re;
	float im;

	/**
	*	Default constructor.
	*/
	scalar(): re(0.0), im(0.0) {}
	/**
	*	Constructor; argument initializes both re & im parts.
	*/
	scalar(float f): re(f), im(f) {}
	/**
	*	Assignment operator;  copies the re & im values from the
	*	rvalue to the lvalue.
	*/
	scalar& operator=(const scalar& s) {
		re = s.re;
		im = s.im;
		return (*this);
	}
	/**
	*	Multiplication operator; multiplies re and im parts of
	*	scalar value by factor, resulting in a new scalar value.
	*/
	scalar& operator*(float f) {
		re *= f;
		im *= f;
		return (*this);
	}
};

/**
 * Spectrometer.
 *
 * Description:\n
 * 	The spectrometer accepts half frames of subchannel data and produces
 * 	baselined spectra for all requested resolutions.  It also inserts
 * 	simulated signals and outputs science data.\n
 * Notes:\n
 * 	It performs spectrometry for both polarizations.\n
 * 	The spectrometer contains very little state information of its own -
 * 	just a wave table for inserting simulated signals.  Virtually all of
 * 	the relevant data is contained in the channel class.
 */
class Spectrometer {
public:
	static Spectrometer *getInstance();
	~Spectrometer();

	Error setup(Activity *act);
	SpecState processHalfFrame(int32_t hf, BufPair *hfBuf);
	void stopSpectrometry(Activity *act);

private:
	static Spectrometer *instance;

	bool hanning;					// use Hanning window for CW
	bool zeroDCBin;
	bool warningSent;
	bool firstHf;					// first half frame
	int32_t subchannels;			// active subchannels
	int32_t startHf;				// starting half frame value
	int32_t baselineHf;				// # of half frames baselined so far
	int32_t halfFrame;
	int32_t resolutions;			// resolutions being created
	int32_t spectraHalfFrames;		// # of half frames to pass to Spectra
	int32_t subchannelPulses;
	int32_t halfFramePulses;
	int32_t totalPulses;
	int32_t baselineReportingRate;
	int32_t waits;
	uint32_t cdDataSize;			// size of CD data buffer
	uint32_t cwDataSize;			// size of CW DADD buffer
	ComplexPair *cdData;			// temp buffer for converted CD data
	uint64_t *cwData;				// temp buffer for CW power data
	Activity *activity;				// current activity
	Channel *channel;
	BaselineLimits blLimits;		// baseline limits
	BaselineStatistics  blStats;	// baseline statistics
	Condition condition;
	ObsData obs;					// observation parameters
	ResInfo resInfo[MAX_RESOLUTIONS];
	struct s {
		int32_t spectrum[MAX_RESOLUTIONS];
		ComplexFloat32 *buf[MAX_RESOLUTIONS];

		s() {
			for (int32_t i = 0; i < MAX_RESOLUTIONS; ++i) {
				spectrum[i] = 0;
				buf[i] = 0;
			}
		}
		void init() {
			for (int32_t i = 0; i < MAX_RESOLUTIONS; ++i) {
				spectrum[i] = 0;
				if (buf[i])
					fftwf_free(buf[i]);
				buf[i] = 0;
			}
		}
	} resSpectra;
#ifdef notdef
	struct s {
		struct _s {
			int32_t spectrum;		// current spectrum (absolute)
			int32_t spectra;		// spectra per half frame
			ComplexFloat32 *buf;	// buffer of nspectra

			_s(): spectrum(0), spectra(0), buf(0) {}
			void init() {
				spectrum = nSpectra = 0;
				if (buf)
					fftwf_free(buf);
				buf = 0;
			}
		} res[MAX_RESOLUTIONS];

		void init() {
			for (int32_t i = 0; i < MAX_RESOLUTIONS; ++i)
				res[i].init();
		}
	} resSpectra;
#endif
	spectra::Spectra spectra;		// spectra library
	HfBufList hfBufs;

	Args *args;
	MsgList *msgList;
	PartitionSet *partitionSet;
	Queue *respQ;
	State *state;

	void setupSpectra();

	void doPolarization(Polarization pol, int32_t hf);
	void processSubchannel(Polarization pol, int32_t subchannel, int32_t hf);

	// confirmation data functions
	void zeroCdData();
	void loadCdPattern(Polarization pol, int32_t subchannel, int32_t hf);
	void computeCdData(ComplexFloat32 *);
	void storeCdData(Polarization pol, int32_t subchannel, int32_t hf);

	// CW data methods
	void zeroCwData();
	void loadCwPattern(Polarization pol, Resolution res, int32_t subchannel,
			int32_t spectrum);
	void computeCwData(Resolution res, const ComplexFloat32 *data);
	void storeCwData(Polarization pol, Resolution res, int32_t subchannel,
			int32_t spectrum);
//	void moveCwData(complexFloat32 *,  complexFloat32 *, int, Resolution );

	// baseline functions
	void computeBaseline(Polarization pol, int32_t subchannel,
			ComplexFloat32 *hfData, float32_t weighting);

	// pulse data functions
	void storePulseData(Polarization pol, Resolution res, int32_t subchannel,
			int32_t spectrum, float32_t threshold, const ComplexFloat32 *data);

	// science data functions
	void sendBlData(const float32_t *data);
	void sendCdData(const ComplexPair *data);

	void freeHfBufs(int32_t n);
	void freeSpectraBufs();

	void sendScienceData(Polarization pol, int32_t hf);
	void sendBaselineData(Polarization pol, int32_t hf);
	void sendBaseline(Polarization pol, int32_t hf);
	void computeBaselineStats(Polarization pol);
	Error checkBaselineLimits(const BaselineLimits& limits, string& s);
	void sendBaselineMsg(Polarization pol, DxMessageCode code, const string& s);
	void sendBaselineStatistics();

	void sendComplexAmplitudes(Polarization pol, int32_t hf);
	int32_t sendSubchannelData(Polarization pol, int32_t hf,
			const DxScienceData &scienceData);

	// hidden
	Spectrometer();

	// forbidden
	Spectrometer(const Spectrometer&);
	Spectrometer& operator=(const Spectrometer&);
};

}

#endif
