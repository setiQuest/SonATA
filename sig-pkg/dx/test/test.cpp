/*******************************************************************************

 File:    test.cpp
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

/*
 * test.cpp
 */

#include <iostream>
#include <fftw3.h>
#include "ArchiveChannel.h"
#include "Dfb.h"
#include "DxErr.h"
#include "Gaussian.h"
#include "ReadFilter.h"
#include "SmallTypes.h"

using namespace dfb;
using namespace dx;
using namespace gauss;
using std::cout;
using std::endl;

const int32_t samplesPerHf = 512;
int32_t subchannels = 16;
int32_t foldings = 10;
int32_t frames = 64;
int32_t hf = 129;
int32_t tdSamples = 0;
int32_t sigChanBins = 32;
float32_t oversampling = 0.25;
//const float64_t frequency = 0;
float64_t subchannelWidthMHz = 533.333333333333333e-6;
float64_t centerFreqMHz = DEFAULT_FREQ;
float64_t signalFreqMHz = DEFAULT_FREQ;
float64_t signalDriftHz = 0;
float64_t signalErrMHz = 0;
float64_t noisePower = 1;

fftwf_plan plan = 0;

// function declarations
ComplexFloat32 *createTdData(float64_t fMHz, float64_t driftHz,
		int32_t samples, float64_t noise);
ComplexPair *createCdSubchannels(const ComplexFloat32 *td);
ComplexFloat32 *createSubchannels(const ComplexFloat32 *td);
void createSpectrum(const ComplexFloat32 *td, ComplexPair *fd);
void createSpectrum(const ComplexFloat32 *td, ComplexFloat32 *fd,
		int32_t shift);
fftwf_plan createPlan();
float32_t computeAvgPower(ComplexFloat32 *d, int32_t samples);
void compareFpData(const ComplexFloat32 *td0, const ComplexFloat32 *td1,
		int32_t samples);
void compareCdData(const ComplexPair *td0, const ComplexPair *td1,
		int32_t samples);
void parseArgs(int argc, char **argv);

int
main(int argc, char **argv)
{
	parseArgs(argc, argv);

	ArchiveChannel ac;
	size_t size;
	Error err;
	if (err = ac.setup(subchannels, hf, samplesPerHf, hf * samplesPerHf,
			oversampling, centerFreqMHz, subchannelWidthMHz)) {
		cout << "error in setup: " << err << endl;
		exit(0);
	}
	size = ac.getSize();

	int32_t samples = ac.getSamples();

	// compute the number of time-domain samples to create, allowing
	// for the execution of the digital filter bank.
	tdSamples = subchannels * foldings
		+ (hf * samplesPerHf - 1) * (subchannels * (1 - oversampling));

	ComplexFloat32 *tdZero = createTdData(centerFreqMHz, 0, tdSamples, 0);
	ComplexFloat32 *fdZero = createSubchannels(tdZero);

	// create original TD data
	ComplexFloat32 *tdOrig = createTdData(signalFreqMHz, signalDriftHz,
			tdSamples, 0);
	float32_t power = computeAvgPower(tdOrig, samples);
	cout << "average sample power in original td data = " << power << endl;

	ComplexFloat4 *tdOrigF4 = new ComplexFloat4[samples];
	ac.convert(tdOrig, (ComplexPair *) tdOrigF4);

	ComplexFloat32 *tdOrigF32 = new ComplexFloat32[samples];
	ac.convert((ComplexPair *) tdOrigF4, tdOrigF32);

	// test dedrift
	ComplexFloat32 *tdDedrift
			= static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	ac.dedrift(tdOrig, tdDedrift, signalFreqMHz, signalDriftHz);
	power = computeAvgPower(tdDedrift, samples);
	cout << "average sample power in dedrifted td data = " << power << endl;
	cout << "compare zero TD data to dedrifted TD data, FP" << endl;
	compareFpData(tdZero, tdDedrift, samples);

	// extract a signal channel from the original data
	int32_t sigChanSamples;
	ComplexFloat32 *sigChan = 0;
	float64_t widthHz = 22.22;
	ac.extractSignalChannel(sigChan, signalFreqMHz + signalErrMHz,
			signalDriftHz, widthHz, sigChanSamples);
	int32_t spectra = (sigChanSamples / sigChanBins) * 2;
	size = sigChanBins * spectra * sizeof(ComplexFloat32);
	ComplexFloat32 *ft = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	ac.createSignalSpectra(sigChan, ft, sigChanBins, sigChanSamples, true);

	// compute the floating point subchannel data, the reconstruction of the
	// floating point time domain data, and the rebuilding of the
	// floating point subchannel data.  Then compare the results.
	// create original subchannels
	cout << endl << "ComplexFloat32" << endl;
	ComplexFloat32 *ffd = createSubchannels(tdOrig);
	size = ac.getSize();
	ComplexFloat32 *ftd = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	// create pass 1 TD data from original subchannels
	ac.create(ffd);
	power = ac.getTdData(ftd);
	cout << "compare original TD data to pass 1 TD data, FP" << endl;
	cout << "average sample power in pass 1 TD data = " << power << endl;
	compareFpData(tdOrig, ftd, samples);
	// create pass 1 subchannels from pass 1 TD data
	ComplexFloat32 *ffd1 = createSubchannels(ftd);
	cout << "compare original subchannels to pass 1 subchannels, FP";
	cout << endl;
	compareFpData(ffd, ffd1, subchannels * hf * samplesPerHf);
	cout << endl;

	// compute the ComplexPair (4-bit integer complex) subchannel data,
	// the reconstruction of the floating point time domain data, and
	// the rebuilding of the floating point subchannel data, and compare
	// the results.
	// compute original CD subchannels
#if FLOAT4_ARCHIVE
	cout << "ComplexFloat4" << endl;
#else
	cout << "ComplexPair" << endl;
#endif
#ifdef notdef
	ComplexPair *ifd = createCdSubchannels(tdOrig);
	ComplexFloat32 *itd = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	// create pass 1 TD data from original CD subchannels
	power =  ac.create(ifd, itd);
	cout << "compare original TD data to pass 1 TD data, FP" << endl;
	cout << "average sample power in pass1 TD data = " << power << endl;
	cout << "compare original TD data to pass 1 TD data, int" << endl;
	compareFpData(tdOrig, itd, samples);
	// create pass 1 CD subchannels from pass 1 TD data
	ComplexPair *ifd1 = createCdSubchannels(itd);
	cout << "compare original subchannels to pass 1 subchannels, int";
	cout << endl;
	compareCdData(ifd, ifd1, subchannels * hf * samplesPerHf);
#endif
#ifdef notdef
	// create pass 2 TD data from pass 1 CD subchannels
	ComplexFloat32 *itd1 = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	ac.create(ifd1, itd1);
	cout << "compare pass 2 TD data to pass 1 TD data, int" << endl;
	compareFpData(itd, itd1, samples);
	// create pass 2 CD subchannels from pass 2 TD data
	ComplexPair *ifd2 = createCdSubchannels(itd1);
	cout << "compare pass 1 subchannels to pass 2 subchannels, int";
	cout << endl;
	compareCdData(ifd1, ifd2, subchannels * hf * samplesPerHf);
	ComplexFloat32 *itd2 = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	ac.create(ifd2, itd2);
	compareFpData(itd1, itd2, samples);
	ComplexPair *ifd3 = createCdSubchannels(itd1);
	compareCdData(ifd2, ifd3, subchannels * hf * samplesPerHf);
	ComplexFloat32 *itd3 = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	ac.create(ifd3, itd3);
	compareFpData(itd2, itd3, samples);
#endif
#ifdef notdef
	// convert the time domain data to 4-bit integer complex; the conversion
	// must be done before transmission of data to the archiver.
	size = ac.getSamples() * sizeof(ComplexPair);
	ComplexPair *cd = static_cast<ComplexPair *> (fftwf_malloc(size));
	ac.convert(itd, cd);
#endif
}

/**
 * Create a time series of input data.
 *
 * Description:\n
 * 	Creates a time series of complex floats representing a signal at
 * 	a given frequency.
 */
ComplexFloat32 *
createTdData(float64_t fMHz, float64_t driftHz, int32_t samples,
		float64_t noise)
{
	size_t size = samples * sizeof(ComplexFloat32);
	ComplexFloat32 *td = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	float64_t dfMHz = fMHz - centerFreqMHz;
	float64_t widthMHz = subchannels * subchannelWidthMHz;
	Gaussian gen;
	// no noise, just signal
	gen.setup(0, widthMHz, noise);
	gen.addCwSignal(dfMHz, driftHz, 1);
	gen.getSamples(td, samples);
	return (td);
}

/**
 * Create a set of subchannels from the time samples.
 *
 * Description:\n
 * 	Performs an FFT to create the subchannel data, then converts from
 * 	floating point complex to ComplexPair, which is 4-bit integer complex.
 */
ComplexPair *
createCdSubchannels(const ComplexFloat32 *td)
{
	size_t size = subchannels * hf * samplesPerHf * sizeof(ComplexPair);
	ComplexPair *fd = static_cast<ComplexPair *> (fftwf_malloc(size));

	if (plan)
		fftwf_destroy_plan(plan);
	plan = createPlan();
	int32_t iStride = subchannels - subchannels * oversampling;
	int32_t oStride = 1;
	int32_t spectra = hf * samplesPerHf;
	for (int32_t i = 0; i < spectra; ++i)
		createSpectrum(td + i * iStride, fd + i * oStride);
	return (fd);
}

/**
 * Create a set of subchannels.
 *
 * Description:\n
 * 	Creates a set of filtered, oversampled subchannels from the time-domain
 * 	data.  A DFB is created and called to perform the subchannelization.
 */
ComplexFloat32 *
createSubchannels(const ComplexFloat32 *td)
{
	size_t size = subchannels * hf * samplesPerHf * sizeof(ComplexFloat32);
	ComplexFloat32 *fd = static_cast<ComplexFloat32 *> (fftwf_malloc(size));

#ifndef notdef
	Dfb dfb;
	FilterSpec filterSpec;
	string filter("../../filters/LS16c10f25o");
	ReadFilter readFilter;
	readFilter.readFilter(filter, filterSpec);
	dfb.setup(subchannels, subchannels * oversampling, foldings,
			hf * samplesPerHf);
	const ComplexFloat32 *in[1];
	in[0] = td;
	ComplexFloat32 *out[subchannels];
	int32_t half = subchannels / 2;
	for (int32_t i = 0; i < subchannels; ++i) {
		if (i < half)
			out[i] = fd + (i + half) * hf * samplesPerHf;
		else
			out[i] = fd + (i - half) * hf * samplesPerHf;
//		out[i] = fd + i * hf * samplesPerHf;
	}
	dfb.iterate(in, 1, tdSamples, out);
#else
	if (plan)
		fftwf_destroy_plan(plan);
	plan = createPlan();
	int32_t overlap = subchannels * oversampling;
	int32_t iStride = subchannels - overlap;
	int32_t oStride = 1;
	int32_t spectra = hf * samplesPerHf;
	for (int32_t i = 0; i < spectra; ++i) {
		createSpectrum(td + i * iStride, fd + i * oStride,
				(i * overlap) % subchannels);
	}
#endif

	return (fd);
}

/**
 * Create a spectrum of subchannels.
 *
 * Description:\n
 *
 */
void
createSpectrum(const ComplexFloat32 *td, ComplexPair *fd)
{
	ComplexFloat32 spectrum[subchannels];

	fftwf_execute_dft(plan, (fftwf_complex *) td, (fftwf_complex *) spectrum);
	int32_t stride = hf * samplesPerHf;
	float32_t scale = 1 / sqrt(subchannels);
	int32_t half = subchannels / 2;
	for (int32_t i = 0; i < subchannels; ++i) {
		spectrum[i] *= scale;
#if FLOAT4_ARCHIVE
		ComplexFloat4 v(spectrum[i]);
		if (i < half)
			fd[(i+half)*stride] = v;
		else
			fd[(i-half)*stride] = v;
#else
		int32_t re = (int32_t) lrintf(spectrum[i].real());
		int32_t im = (int32_t) lrintf(spectrum[i].imag());
		if (re > 7)
			re = 7;
		if (re < -7)
			re = -7;
		if (im > 7)
			im = 7;
		if (im < -7)
			im = -7;
		if (i < half)
			fd[(i+half)*stride].pair = (re << 4) | (im & 0xf);
		else
			fd[(i-half)*stride].pair = (re << 4) | (im & 0xf);
#endif
	}
}
void
createSpectrum(const ComplexFloat32 *td, ComplexFloat32 *fd, int32_t shift)
{
	ComplexFloat32 t[subchannels];
	ComplexFloat32 spectrum[subchannels];

	// rotate the time samples to correct phase
	int32_t ofs = subchannels - shift;
	memcpy(t, td + shift, ofs * sizeof(ComplexFloat32));
	memcpy(t + ofs, td, shift * sizeof(ComplexFloat32));
	fftwf_execute_dft(plan, (fftwf_complex *) t, (fftwf_complex *) spectrum);
	int32_t stride = hf * samplesPerHf;
	float32_t scale = 1 / sqrt(subchannels);
	int32_t half = subchannels / 2;
	for (int32_t i = 0; i < subchannels; ++i) {
		spectrum[i] *= scale;
		if (i < half)
			fd[(i+half)*stride] = spectrum[i];
		else
			fd[(i-half)*stride] = spectrum[i];
	}
}

fftwf_plan
createPlan()
{
	size_t size = subchannels * sizeof(ComplexFloat32);
	ComplexFloat32 *in = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	ComplexFloat32 *out = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	fftwf_plan p = fftwf_plan_dft_1d(subchannels,
			(fftwf_complex *) in, (fftwf_complex *) out, FFTW_FORWARD,
			FFTW_ESTIMATE);
	return (p);
}

float32_t
computeAvgPower(ComplexFloat32 *d, int32_t samples)
{
	float32_t sum = 0;
	for (int32_t i = 0; i < samples; ++i)
		sum += std::norm(d[i]);
	return (sum / samples);
}
void
compareFpData(const ComplexFloat32 *td0, const ComplexFloat32 *td1,
		int32_t samples)
{
	ComplexFloat32 maxDiff(0, 0);
	bool fail = false;
	int32_t reIndex = 0, imIndex = 0;
	// there may be scaling differences between the two sequences, so
	// normalize by the first sample in each sequence
	float32_t scale = 0;
	for (int32_t i = 0; i < samples; ++i) {
		if (std::norm(td0[i])) {
			scale = 1.0 / sqrt(std::norm(td1[i]) / std::norm(td0[i]));
			break;
		}
	}
	Assert(scale);
	for (int32_t i = 0; i < samples; ++i) {
		ComplexFloat32 diff = td0[i] - td1[i] * scale;
		float64_t re = fabs(diff.real());
		float64_t im = fabs(diff.imag());
		if (re > maxDiff.real()) {
			reIndex = i;
			maxDiff.real() = re;
		}
		if (im > maxDiff.imag()) {
			imIndex = i;
			maxDiff.imag() = im;
		}
		if ((re > 5e-6 || im > 5e-6) && !fail) {
			fail = true;
			cout << "failed at index " << i << ", orig = " << td0[i];
			cout << ", new = " << td1[i] << endl;
		}
	}
	cout << "maximum diff = " << maxDiff << " at index re " << reIndex;
	cout << ", im " << imIndex << endl;
	if (!fail)
		cout << "compare succeeded" << endl;
	else
		cout << "compare failed" << endl;
}

void
compareCdData(const ComplexPair *cd0, const ComplexPair *cd1, int32_t samples)
{
	ComplexFloat32 maxDiff(0, 0);
	int32_t differences = 0;
	for (int32_t i = 0; i < samples; ++i) {
		if (cd0[i].pair != cd1[i].pair)
			++differences;
	}
	cout << differences << " differences in " << samples << " samples" << endl;
	if (!differences)
		cout << "compare succeeded" << endl;
	else
		cout << "compare failed" << endl;

}

/**
 * Parse the argument list.
 */
static string usageString = "test [-h] [-c centerFreq] [-d drift] [-f frames] [-n subchannels] [-o oversampling] [-s signalFreq] [-w subchannelwidth]\n\
		-h: print usage\n\
		-c centerFreq: center frequency of archive channel in MHz\n\
		-d drift: drift of signal in Hz/s\n\
		-e err: signal err in MHz\n\
		-f frames: # of frames\n\
		-n subchannels: # of subchannels\n\
		-o oversampling: percentage of oversampling of subchannels\n\
		-s signalFreq: signal frequency in MHz\n\
		-w subchannelWidth: subchannel width in MHz\n\\n";

void usage()
{
	cout << usageString << endl;
	exit(0);
}

void
parseArgs(int argc, char **argv)
{
	bool done = false;
	const char *optstring = "hc:d:e:f:n:o:s:w:";
	extern char *optarg;
//	extern int optind;
//	extern int optopt;
	extern int opterr;

	opterr = 0;
	while (!done) {
		switch (getopt(argc, argv, optstring)) {
		case -1:
			done = true;
			break;
		case 'c':
			centerFreqMHz = atof(optarg);
			break;
		case 'd':
			signalDriftHz = atof(optarg);
			break;
		case 'e':
			signalErrMHz = atof(optarg);
			break;
		case 'f':
			frames = atoi(optarg);
			hf = 2 * frames + 1;
			break;
		case 'h':
			usage();
			break;
		case 'n':
			subchannels = atoi(optarg);
			break;
		case 'o':
			oversampling = atof(optarg);
			break;
		case 's':
			signalFreqMHz = atof(optarg);
			break;
		case 'w':
			subchannelWidthMHz = atof(optarg);
			break;
		default:
			cout << "invalid option " << optarg << endl;
			usage();
			break;
		}
	}
	int32_t overlap = subchannels * oversampling;
	if (overlap != subchannels * oversampling) {
		cout << "subchannnels * oversampling must be an integer" << endl;
		cout << "subchannels = " << subchannels << ", oversampling = ";
		cout << oversampling << endl;
		exit(0);
	}
}
