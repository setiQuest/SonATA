/*******************************************************************************

 File:    Gaussian.cpp
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
// Gaussian sample generator.
//
// Uses Mersenne Twister by Takuji Nishimura and Makoto Matsumoto.
//

#include "Gaussian.h"
#include "Mersenne.h"

namespace gauss {

Gaussian::Gaussian(int32_t seed_): seed(seed_), sampleCnt(0),
		bandwidthMHz(DEFAULT_BANDWIDTH), avgPower(DEFAULT_POWER), power(0, 0),
		sum(0, 0)
{
	setSeed(seed);
}

Gaussian::~Gaussian()
{
}

/**
 * Set up the noise/signal generator
 *
 * Description:\n
 * 	Set the parameters for the noise generator, including seed, average
 *  noise power and bandwidth.\n\n
 * Notes:\n
 *  This initializes the signal generator, resetting all counters and
 *  summing registers.  Any previously added signals are removed.
 */
void
Gaussian::setup(int32_t seed_, float64_t bandwidthMHz_, float64_t avgPower_)
{
	setSeed(seed_);
	setBandwidth(bandwidthMHz_);
	setNoisePower(avgPower_);
	sampleCnt = 0;
	power = ComplexFloat64(0, 0);
	sum = ComplexFloat64(0, 0);
	for (SigList::iterator s = signals.begin(); s != signals.end(); ++s)
		delete s->sig;
	signals.clear();
}

/**
 * Set the seed and initialize the RNG.
 *
 * Notes:\n
 * 	If the seed is 0, the current time is used as the seed.
 */
void
Gaussian::setSeed(int32_t seed_)
{
	if (!(seed = seed_))
		seed = static_cast<int32_t> (time(NULL));
	init_genrand64(seed);
}

/**
 * Add a CW signal to the signal list.
 *
 * Description:\n
 * 	Adds a CW signal of constant drift to the list of signals which are to
 * 	be inserted into the sample data.  Starting frequency, drift and SNR
 * 	are specified.\n\n
 * Notes:\n
 * 	Injection of signals without specifying the correct bandwidth for the
 * 	sample stream will produce incorrect results.\n
 * 	The meaning of SNR is dependent on the noise power specified by setup:
 * 	if noise power is non-zero, then SNR is the signal/noise ratio of the
 * 	inserted signal;  if noise power is zero, then SNR is the absolute
 * 	signal power in a 1Hz bin.
 */
void
Gaussian::addCwSignal(float64_t freq_, float64_t drift_, float64_t snr_)
{
	CwSig *cwSig = new CwSig(bandwidthMHz, avgPower, freq_, drift_, snr_);
	Sig sig(CwSignal, cwSig,
			(void (*)(BasicSig *, ComplexFloat64&, uint64_t)) &genCwSignal);
	signals.push_back(sig);
}

/**
 * Add a pulse signal to the signal list.
 *
 * Description:\n
 * 	Adds a pulse signal of constant drift and period to the list of signals
 * 	which are to be inserted into the sample data.  Starting frequency,
 * 	drift, SNR, start time, pulse on time and pulse off time are specified.\n\n
 * Notes:\n
 * 	Start time is currently ignored.\n
 * 	Injection of signals without specifying the correct bandwidth for the
 * 	sample stream will produce incorrect results.\n
 * 	The meaning of SNR is dependent on the noise power specified by setup:
 * 	if noise power is non-zero, then SNR is the signal/noise ratio of the
 * 	inserted signal;  if noise power is zero, then SNR is the absolute
 * 	signal power in a 1Hz bin.
 *
 */
void
Gaussian::addPulseSignal(float64_t freq_, float64_t drift_, float64_t snr_,
		float64_t tStart_, float64_t tOn_, float64_t tOff_)
{
	PulseSig *pulseSig = new PulseSig(bandwidthMHz, avgPower, freq_, drift_,
			snr_, tStart_, tOn_, tOff_);
	Sig sig(PulseSignal, pulseSig,
			(void (*)(BasicSig *, ComplexFloat64&, uint64_t)) &genPulseSignal);
	signals.push_back(sig);
}

/**
 * Add a user-defined signal.
 *
 * Description:\n
 * 	Adds the control structure and callback function for a user-defined
 * 	signal type.
 */
void
Gaussian::addUserSignal(BasicSig *basicSig,
		void (*genSig)(BasicSig *, ComplexFloat64&, uint64_t))
{
	Sig sig(UserSignal, basicSig, genSig);
	signals.push_back(sig);
}

/**
* Create a block of ComplexPair (4-bit real, 4-bit imaginary) samples
*
* Description:\n
*	Generates a block (typically a packet full) of samples consisting
*	of noise and whatever signals have been specified.\n\n
* Notes:\n
*	The samples generated are 4-bit real, 4-bit imaginary.\n
*	Values outside the range -7 to 7 are saturated.
*
* @param	data pointer to the storage area
* @param	n number of samples to generate and store
*/
void
Gaussian::getSamples(ComplexPair *data, int32_t n)
{
	const int32_t max = 7, min = -7;
	for (int32_t i = 0; i < n; ++i) {
		ComplexFloat64 f = getSample();

		// using 4-bit integers, so saturate the output
		int32_t re = (int32_t) lrint(f.real());
		int32_t im = (int32_t) lrint(f.imag());
		if (re > max)
			re = max;
		else if (re < min)
			re = min;
		if (im > max)
			im = max;
		else if (im < min)
			im = min;
		data[i].pair = (re << 4) | (im & 0xf);
	}
}

/**
* Create a block of ComplexFloat4 (4-bit real, 4-bit imaginary) microfloat
* 	samples.
*
* Description:\n
*	Generates a block (typically a packet full) of samples consisting
*	of noise and whatever signals have been specified.\n\n
* Notes:\n
*	The samples generated are 4-bit real, 4-bit imaginary.\n
*	Each value is a microfloat (float4_t); values outside the representable
*	range are saturated..
*
* @param	data pointer to the storage area
* @param	n number of samples to generate and store
*/
void
Gaussian::getSamples(ComplexFloat4 *data, int32_t n)
{
	for (int32_t i = 0; i < n; ++i) {
		ComplexFloat64 f = getSample();
		data[i] = ComplexFloat4(f);
	}
}

/**
* Create a block of ComplexInt8 (8-bit integer) samples
*
* Description:\n
*	Generates a block (typically a packet full) of samples consisting
*	of noise and whatever signals have been specified.\n\n
* Notes:\n
*	The samples generated are 8-bit real, 8-bit imaginary.\n
*	Values outside the range -127 to 127 are saturated.
*
* @param	data pointer to the storage area
* @param	n number of samples to generate and store
*/
void
Gaussian::getSamples(ComplexInt8 *data, int32_t n)
{
	const int32_t max = SCHAR_MAX, min = -SCHAR_MAX;
	for (int32_t i = 0; i < n; ++i) {
		ComplexFloat64 f = getSample();

		// using 4-bit integers, so saturate the output
		int32_t re = (int32_t) lrint(f.real());
		int32_t im = (int32_t) lrint(f.imag());
		if (re > max)
			re = max;
		else if (re < min)
			re = min;
		if (im > max)
			im = max;
		else if (im < min)
			im = min;
		data[i] = ComplexInt8(re, im);
	}
}

/**
* Create a block of ComplexInt16 (16-bit integer) samples
*
* Description:\n
*	Generates a block (typically a packet full) of samples consisting
*	of noise and whatever signals have been specified.\n\n
* Notes:\n
*	The samples generated are 16-bit real, 16-bit imaginary.\n
*	Values outside the range -32767 to 32767 are saturated.
*
* @param	data pointer to the storage area
* @param	n number of samples to generate and store
*/
void
Gaussian::getSamples(ComplexInt16 *data, int32_t n)
{
	const int32_t max = SHRT_MAX, min = -SHRT_MAX;
	for (int32_t i = 0; i < n; ++i) {
		ComplexFloat64 f = getSample();

		// using 4-bit integers, so saturate the output
		int32_t re = (int32_t) lrint(f.real());
		int32_t im = (int32_t) lrint(f.imag());
		if (re > max)
			re = max;
		else if (re < min)
			re = min;
		if (im > max)
			im = max;
		else if (im < min)
			im = min;
		data[i] = ComplexInt8(re, im);
	}
}

/**
* Create a block of ComplexFloat32 (32-bit floating point) samples
*
* Description:\n
*	Generates a block (typically a packet full) of samples consisting
*	of noise and whatever signals have been specified.\n\n
* Notes:\n
*	The samples generated are 8-bit real, 8-bit imaginary.\n
*	Values outside the range -FLT_MAX to FLT_MAX are saturated.
*
* @param	data pointer to the storage area
* @param	n number of samples to generate and store
*/
void
Gaussian::getSamples(ComplexFloat32 *data, int32_t n)
{
	const float64_t max = FLT_MAX, min = -FLT_MAX;
	for (int32_t i = 0; i < n; ++i) {
		ComplexFloat64 f = getSample();

		// using 4-bit integers, so saturate the output
		float64_t re = f.real();
		float64_t im = f.imag();
		if (re > max)
			re = max;
		else if (re < min)
			re = min;
		if (im > max)
			im = max;
		else if (im < min)
			im = min;
		data[i] = ComplexFloat32(re, im);
	}
}

/**
* Create a block of complex double (64-bit) samples
*
* Description:\n
*	Generates a block (typically a packet full) of samples consisting
*	of noise and whatever signals have been specified.\n\n
* Notes:\n
*	The samples generated are complex double-precision floats (64-bit real,
*	64-bit imaginary).
*
* @param	data pointer to the storage area
* @param	n number of samples to generate and store
*/
void
Gaussian::getSamples(ComplexFloat64 *data, int32_t n)
{
	for (int32_t i = 0; i < n; ++i)
		data[i] = getSample();
}

/**
 * Compute a complex double (64-bit) sample.
 *
 * Description:\n
 * 	Computes a noise sample with average power of 1, then superimposes
 * 	on it the set of signals specified.  When all signals have been
 * 	added, scale by the desired noise power.\n\n
 * Notes:\n
 * 	Sample statistics (total sample power, total sample sum) are maintained;
 * 	they can be normalized by dividing by the total number of samples.
*/
ComplexFloat64
Gaussian::getSample()
{
	// get the noise-only sample
	ComplexFloat64 sample(dirtyGauss());
	sample *= sqrt(avgPower);

	// add in all the signals
	for (uint32_t i = 0; i < signals.size(); ++i) {
		Sig& sig = signals[i];
		sig.genSignal(sig.sig, sample, sampleCnt);
	}
	++sampleCnt;
	sum += sample;
	power.real() += sample.real() * sample.real();
	power.imag() += sample.imag() * sample.imag();
	return (sample);
}

/**
 * Compute a noise sample.
 *
 * Description:\n
 * 	Uses Mersenne Twister pseudo-random noise generator to produce a
 * 	complex Gaussian noise sample of average power 1.\n\n
 * Notes:\n
 * 	Mersenne Twister is a fast PRNG which produces virtually no artifacts.
 */
ComplexFloat64
Gaussian::dirtyGauss()
{
	float64_t v1, v2, s;
	do {
		float64_t u1 = genrand64_real2();
		float64_t u2 = genrand64_real2();
		v1 = 2 * u1 - 1;
		v2 = 2 * u2 - 1;
		s = v1 * v1 + v2 * v2;
	} while (s >= 1);

	float64_t r = sqrt(-2 * log(s) / s);
	float64_t re = r * v1;
	float64_t im = r * v2;
	ComplexFloat64 f(re / M_SQRT2, im / M_SQRT2);
	return (f);
}

}
