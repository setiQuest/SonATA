/*******************************************************************************

 File:    ArchiveChannel.cpp
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
 * ArchiveChannel: create an archive channel from a set of subchannels.
 */
#include "ArchiveChannel.h"
#include "DxErr.h"
#include "SmallTypes.h"

namespace dx {

ArchiveChannel::ArchiveChannel(): nSubchan(0), hf(0), samplesPerHf(0),
		oversampling(0), freqMHz(0), spectrum(0), driftBuf(0)
{
}

ArchiveChannel::~ArchiveChannel()
{
	in.reset();
	ac.reset();
	ss.reset();
	bin.reset();
	if (driftBuf)
		fftwf_free(driftBuf);
}

/**
 * Set up to create archive channels.
 *
 * Description:\n
 * 	If the number of subchannels has changed, destroys any existing plan
 * 	and spectrum buffer and creates new ones.  Computes the minimum DFT
 * 	length for removal of subchannel oversampling
 * @param	nSubchan_ number of subchannels to combine
 * @param	hf_ number of half frames
 * @param	samplesPerHf_ number of samples per subchannel per half frame
 * @param	stride_ # of samples between subchannels
 * @param	oversampling_ percentage of oversampling in a subchannel
 * @param	freqMHz_ center frequency of the center subchannel
 * @param	subchanWidthMHz_ width of each subchannel in MHz (after
 * 			oversampling has bee removed)
 */
Error
ArchiveChannel::setup(int32_t nSubchan_, int32_t hf_, int32_t samplesPerHf_,
		int32_t stride_, float32_t oversampling_, float64_t freqMHz_,
		float64_t subchanWidthMHz_)
{
	if (nSubchan != nSubchan_ || hf != hf_ || samplesPerHf != samplesPerHf_
			|| oversampling != oversampling_) {
		nSubchan = nSubchan_;
		hf = hf_;
		samplesPerHf = samplesPerHf_;
		oversampling = oversampling_;
		in.reset();
		ac.reset();
		bin.reset();
		if (spectrum) {
			fftwf_free(spectrum);
			spectrum = 0;
		}

		// set input subchannel parameters
		in.fftLen = 1;
		in.samples = hf * samplesPerHf;
		in.spectra = in.samples;
		in.stride = stride_;
		in.widthMHz = subchanWidthMHz_;
		size_t size = nSubchan * in.samples * sizeof(ComplexFloat32);
		in.data = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
		Assert(in.data);

		// set archive channel parameters
		ac.fftLen = nSubchan;
		ac.spectra = in.samples;
		ac.samples = ac.spectra * ac.fftLen;
		ac.stride = ac.fftLen;
		ac.widthMHz = nSubchan * in.widthMHz;
		size = ac.samples * sizeof(ComplexFloat32);
		ac.data = static_cast<ComplexFloat32 *>  (fftwf_malloc(size));
		Assert(ac.data);

		size = ac.fftLen * sizeof(ComplexFloat32);
		spectrum = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
		Assert(spectrum);

		createPlan(ac.plan, ac.data, ac.fftLen, FFTW_BACKWARD);
		if (driftBuf) {
			fftwf_free(driftBuf);
			driftBuf = 0;
		}
	}
	freqMHz = freqMHz_;
	return (0);
}

/**
 * Create an archive channel from a set of subchannels.
 *
 * Description:\n
 * 	Performs an inverse FFT from several subchannels of confirmation
 * 	data to create a wider channel for coherent detection, secondary
 * 	confirmation or simply for archiving.  The output is a set of
 * 	time samples for a single archive channel corresponding to the
 * 	set of subchannels specified.  The time samples are complex
 * 	floating point.\n\n
 * Notes:\n
 * 	This channel is created whenever a channel wider than a single
 * 	subhchannel is needed for processing or archiving.  It is used in
 * 	two distinct situations during signal detection:: (1)
 * 	when a signal is detected with DADD, a coherent detection is performed
 * 	using the DADD bin and drift to more accurately describe the signal's
 * 	starting bin, width and SNR.  (2) When a signal is detected in another
 * 	beam, the detector may be asked to do a restricted, coherent search
 * 	for the signal to determine whether or not it is visible.  This is
 * 	known as secondary or confirmation detection.\n
 *	Synthesis of the channel is simply an n-point inverse FFT to generate
 *	n time samples of a wider channel with the same oversampling as the
 *	original subchannels.\n
 *	An arbitrary number of adjacent subchannels may be combined to create
 *	the archive channel.\n
 *  There are two versions of this function.  One takes confirmation
 *  data (also known as complex amplitudes) in 4-bit integer complex form.
 *  This is the standard format used by the DX.  The second version takes
 *  floating-point complex, which allows us to directly reconstruct the
 *  time series data from a set of subchannels.  This version is primarily
 *  used during debugging, to ensure that the function is working by allowing
 * 	accurate reconstruction of both time and frequency domain values.
 */
/**
 * Create an archive channel from 4-bit integer complex subchannels.\n\n
 * Notes:\n
 * 	Returns the average power in a time-domain sample.
 *
 *  @param		scData pointer to first subchannel of frequency data.  The
 *  			data for a single subchannel is assumed to be contiguous
 *  			in memory, with in.stride samples between subchannels.
 *  @param		acData pointer to archive channel data output (time domain).
 *  			The output data is assumed to be a single contiguous set
 *  			in memory, with oStride samples between spectra.  Note
 *  			that oStride will typically be shorter than the fft length
 *  			because of oversampling of the subchannels.
 */
float32_t
ArchiveChannel::create(const ComplexPair *scData, bool baseline)
{
	// convert the data from all subchannels into the intermediate buffer
	for (int32_t i = 0; i < nSubchan; ++i)
		convert(scData + in.stride * i, in.data + in.samples * i, in.samples);
	return (createChannel(baseline));
}

/**
 * Create an archive channel from floating-point complex subchannels.
 *
 * Description:\n
 * 	Removes oversampling from the subchannel data, then performs an
 * 	inverse FFT to combine the subchannels into a wider archive channel.\n
 * Notes:\n
 * 	Much more accurate reconstruction of time series than integer version
 * 	above.  Returns the average power in a time-domain sample.
 *
 *  @param		scData pointer to first subchannel of frequency data.  The
 *  			data for a single subchannel is assumed to be contiguous
 *  			in memory, with in.stride samples between subchannels.
 *  @param		acData pointer to archive channel data output (time domain).
 *  			The output data is assumed to be a single contiguous set
 *  			in memory, with oStride samples between spectra.  Note
 *  			that oStride will typically be shorter than the fft length
 *  			because of oversampling of the subchannels.
 */
float32_t
ArchiveChannel::create(const ComplexFloat32 *scData, bool baseline)
{
	// copy the data from all subchannels to the intermediate buffer
	size_t size = in.samples * sizeof(ComplexFloat32);
	for (int32_t i = 0; i < nSubchan; ++i)
		memcpy(in.data + in.samples * i, scData + in.stride * i, size);
	return (createChannel(baseline));
}

/**
 * Get the archive channel time-domain data.
 *
 * Description:\n
 * 	Stores the archive channel time-domain data in the destination,
 * 	returning the baseline value for the data.
 */
float32_t
ArchiveChannel::getTdData(ComplexFloat32 *acData)
{
	size_t size = ac.samples * sizeof(ComplexFloat32);
	memcpy(acData, ac.data, size);
	return (computeBaseline(ac.data, ac.samples));
}

/**
 * Create the archive channel.
 */
float32_t
ArchiveChannel::createChannel(bool baseline)
{
	// create archive channel time samples, one spectrum at a time
	ComplexFloat32 *sd = in.data;
	ComplexFloat32 *ad = ac.data;
	for (int32_t i = 0; i < ac.spectra; ++i) {
		assembleSpectrum(sd);
		createTimeSamples(ad);
		++sd;
		ad += ac.stride;
	}
	float32_t b = computeBaseline(ac.data, ac.samples);
	if (baseline)
		baselineChannel(b);
	return (computeBaseline(ac.data, ac.samples));
}

/**
 * Baseline the archive channel.
 *
 * Description:\n
 * 	Applies the baseline value to the archive channel time samples.
 */
void
ArchiveChannel::baselineChannel(float32_t base)
{
	Assert(base);
	float32_t factor = 1 / sqrt(base);
	for (int32_t i = 0; i < ac.samples; ++i)
		ac.data[i] *= factor;
}

/**
 * Convert the archive channel from floating point complex to ComplexPair.
 *
 * Description:\n
 * 	Converts the entire set of archive channel samples from floating point
 * 	complex to 4-bit integer complex.\n\n
 * Notes:\n
 * 	The caller must supply an output array which is large enough to hold
 * 	all the data points.  The total number of data points can be determined
 * 	by calling getSamples.
 *
 * @param	acData the input channel data in complex floating point format.
 * @param	cdData the output channel data in 4-bit integer complex format.
 * @param	n the number of samples to convert (0 => use ac.samples)
 */
void
ArchiveChannel::convert(const ComplexFloat32 *acData, ComplexPair *cdData,
		int32_t n)
{
	if (!n)
		n = ac.samples;
	for (int32_t i = 0; i < n; ++i) {
#if FLOAT4_ARCHIVE
		ComplexFloat4 v(acData[i]);
		cdData[i] = (ComplexPair) v;
#else
		int32_t re = (int32_t) lrintf(acData[i].real());
		int32_t im = (int32_t) lrintf(acData[i].imag());
		if (re > 7)
			re = 7;
		if (re < -7)
			re = -7;
		if (im > 7)
			im = 7;
		if (im < -7)
			im = -7;
		cdData[i].pair = (re << 4) | (im & 0xf);
#endif
	}
}

/**
 * Convert the archive channel from archive format to ComplexFloat32.
 *
 * Description:\n
 * 	Converts the entire set of archive channel samples from floating point
 * 	complex to 4-bit integer complex.\n\n
 * Notes:\n
 * 	The caller must supply an output array which is large enough to hold
 * 	all the data points.  The total number of data points can be determined
 * 	by calling getSamples.
 *
 * @param	cdData the input channel data in 4-bit complex format.
 * @param	acData the output channel data in complex floating point format.
 * @param	n number of samples to convert (0 => use ac.samples)
 */
void
ArchiveChannel::convert(const ComplexPair *cdData, ComplexFloat32 *acData,
		int32_t n)
{
	if (!n)
		n = ac.samples;
	for (int32_t i = 0; i < n; ++i) {
#if FLOAT4_ARCHIVE
		const ComplexFloat4 *f4Data = (ComplexFloat4 *) cdData;
		acData[i] = ComplexFloat32((float32_t) f4Data[i].real(),
				(float32_t) f4Data[i].imag());
#else
		int32_t re = cdData[i].pair >> 4;
		int32_t im = cdData[i].pair & 0xf;
		if (re & 0x8)
			re |= 0xfffffff0;
		if (im & 0x8)
			im |= 0xfffffff0;
		acData[i] = ComplexFloat32(re, im);
#endif
	}
}

/**
 * Assemble the complex spectrum to be inverse transformed.
 *
 * Description:\n
 * 	Takes one sample from each of the subchannels, converts them from
 * 	4-bit integer complex to floating point complex, and rearranges them
 * 	into normal FFT output format, so that an inverse transform can be
 * 	run to recreate the original time samples.\n\n
 * Notes:\n
 * 	A spectrum buffer is preallocated to hold the frequency-domain samples.
 */
/**
 * Assemble a spectrum from 4-bit integer complex subchannels.
 */
void
ArchiveChannel::assembleSpectrum(const ComplexPair *scData)
{
	int32_t half = nSubchan / 2;
	for (int32_t i = 0; i < nSubchan; ++i, scData += in.stride) {
#if FLOAT4_ARCHIVE
		const ComplexFloat4 *data = (const ComplexFloat4 *) scData;
		ComplexFloat32 v((float32_t) data->real(), (float32_t) data->imag());
#else
		uint8_t val = scData->pair;
		int32_t re = (int8_t) val >> 4;
		int32_t im = (int8_t) (val << 4) >> 4;
		ComplexFloat32 v(re, im);
#endif
		if (i < half)
			spectrum[i+half] = v;
		else
			spectrum[i-half] = v;
	}
}

/**
 * Assemble a spectrum from floating-point complex subchannels.
 *
 * Description:\n
 * 	Builds a spectrum from n subchannels of data.  Construction is
 * 	complicated by the fact that the subchannels are oversampled and
 * 	therefore wider than the spacing between subchannels.  This overlap
 * 	of frequency coverage must be removed before the archive channel is
 * 	synthesized.  To remove the oversampling from a subchannel, we
 * 	first perform an FFT on a block of the subchannel's time domain
 * 	data to produce a spectrum.  The bins which represent the oversampled
 * 	region are then discarded (half at each edge of the bandwidth),
 * 	resulting in a spectrum containing a smaller number of bins which
 * 	represent only the actual bandwidth of the subchannel.  This process
 * 	is repeated for all subchannels and the subchannel spectra are assembled
 * 	side by side to create the archive channel spectrum.  An inverse FFT
 * 	is then performed on this spectrum to produce time samples for the
 * 	channel.\n\n
 * Notes:\n
 * 	The length of the subchannel FFT depends upon the amount of oversampling
 * 	used to create the subchannels: the number of discarded bins (= FFTlen
 * 	* oversampling) must be even so that we can discard the same number of
 * 	low and high frequency bins.
 */
void
ArchiveChannel::assembleSpectrum(const ComplexFloat32 *scData)
{
	int32_t half = nSubchan / 2;
	for (int32_t i = 0; i < nSubchan; ++i, scData += in.stride) {
		if (i < half)
			spectrum[i+half] = *scData;
		else
			spectrum[i-half] = *scData;
	}
}

/**
 * Create time samples from the frequency bins.
 *
 * Description:\n
 * 	Given the set of frequency bins for a spectrum, creates the time samples
 * 	which produced those bins.\n\n
 * Notes:\n
 *
 */
void
ArchiveChannel::createTimeSamples(ComplexFloat32 *td)
{
	fftwf_execute_dft(ac.plan, (fftwf_complex *) spectrum,
			(fftwf_complex *) td);
	rescale(td, ac.fftLen, ac.fftLen);
}

/**
 * Create an FFT plan.
 *
 * Description:\n
 * 	Creates a plan for an to be run on each subchannel to remove the
 * 	oversampling.  This is done by performing an FFT, then discarding the
 * 	edge bins which are oversampled, leaving only the actual bandwidth of
 * 	the subchannel.  The length of the FFT and the number of edge bins
 * 	discarded depend upon the amount of oversampling, since we must discard
 * 	an equal number of bins from each edge.  For example, for 25% oversampling
 * 	we do an 8-point FFT and discard 25% of the output bins, or 1 at each
 * 	edge.  For .1875 oversampling, we would do a 32-point FFT discard 3 bins
 * 	each edge.  The FFT is performed on the time samples of each subchannel.\n\n
 * Notes:\n
 * 	FFT length and number of discarded bins are computed during setup.\n
 *
 * @param	plan the created plan
 * @param	spectrum pointer to a buffer pointer allocated by the function
 * @param	n length of the FFT
 * @param	inverse boolean to indicate whether forward or reverse FFT
 */
void
ArchiveChannel::createPlan(fftwf_plan& plan, ComplexFloat32 *spectrum,
		int32_t n, int32_t dir)
{
	if (plan) {
		fftwf_destroy_plan(plan);
		plan = 0;
	}
	Assert(spectrum);
	size_t size = n * sizeof(ComplexFloat32);
	ComplexFloat32 *work = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
	Assert(work);
	plan = fftwf_plan_dft_1d(n, (fftwf_complex *) spectrum,
			(fftwf_complex *) work, dir, FFTW_ESTIMATE);
	Assert(plan);
	fftwf_free(work);
}

/**
 * Rescale the output for the length of the FFT.
 */
void
ArchiveChannel::rescale(ComplexFloat32 *td, int32_t fftLen, int32_t samples)
{
	float32_t factor = 1 / sqrt(fftLen);
	for (int32_t i = 0; i < samples; ++i)
		td[i] *= factor;
}

/**
 * Compute the baseline for the channel.
 *
 * Description:\n
 * 	Called after the channel has been created and rescaled.  Computes the
 * 	average power in a channel bin and returns it to the caller.
 */
float32_t
ArchiveChannel::computeBaseline(ComplexFloat32 *td, int32_t samples)
{
	float32_t sum = 0;
	for (int32_t i = 0; i < samples; ++i)
		sum += std::norm(td[i]);
	return (sum / samples);
}

/**
 * Extract a signal channel from the archive channel.
 *
 * Description:\n
 * 	Dedrifts and frequency shifts  the input archive data to place
 * 	the specified signal at DC, then sums sets of bins to produce
 * 	a signal channel as near as possible to the specified width.\n\n
 * Notes:\n
 * 	The input data is assumed to be an archive channel of time-domain
 * 	data as specified for the class.\n
 * 	The output data must be large enough to hold the time-domain signal
 * 	channel data, which will typically be much smaller than the archive
 * 	channel data.\n
 * 	A working buffer is allocated to hold the dedrifted archive channel.\n
 * 	The channel width will be greater than or equal to the requested width.
 */
int32_t
ArchiveChannel::getSamplesPerSignalSample(float64_t widthHz)
{
	float64_t tmp = MHZ_TO_HZ(ac.widthMHz) / widthHz;
	int32_t samplesPerBlk = (int32_t) lrint(tmp);
	return (samplesPerBlk);
}

int32_t
ArchiveChannel::getSignalSamples(float64_t widthHz)
{
	int32_t samplesPerBlk = getSamplesPerSignalSample(widthHz);
	int32_t blks = ac.samples / samplesPerBlk;
	return (blks);
}

void
ArchiveChannel::extractSignalChannel(ComplexFloat32 *scData,
		float64_t fMHz, float64_t driftHz, float64_t& widthHz, int32_t samples)
{
	Assert(scData);

	int32_t samplesPerBlk = getSamplesPerSignalSample(widthHz);
	int32_t n = getSignalSamples(widthHz);
	Assert(samples <= n);
	widthHz = MHZ_TO_HZ(ac.widthMHz / samplesPerBlk);
	ComplexFloat32 *data = scData;

	// allocate the dedrift buffer if necessary
	if (!driftBuf) {
		size_t size = ac.samples * sizeof(ComplexFloat32);
		driftBuf = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
		Assert(driftBuf);
	}
	dedrift(ac.data, driftBuf, fMHz, driftHz);

	// the dedrifted data can now be summed to get the signal channel, which
	// is at DC.  Rescale by the length of the block.
	for (int32_t i = 0; i < samples; ++i) {
		ComplexFloat32 sum(0, 0);
		ComplexFloat32 *blk = driftBuf + i * samplesPerBlk;
		for (int32_t j = 0; j < samplesPerBlk; ++j)
			sum += blk[j];
		data[i] = sum;
	}
	rescale(data, samplesPerBlk, samples);
}

int32_t
ArchiveChannel::getSignalSpectra(int32_t fftLen, int32_t samples, bool overlap)
{
	int32_t spectra = samples / fftLen;
	if (overlap)
		spectra *= 2;
	return (spectra);
}

/**
 * Extract a single bin from the archive channel.
 */
ComplexFloat32
ArchiveChannel::extractBin(float64_t fMHz, float64_t wHz, int32_t spectrum,
		bool overlap)
{
	int32_t fftLen = lrint(MHZ_TO_HZ(ac.widthMHz) / wHz);
	bin.spectra = 1;
	bin.widthMHz = ac.widthMHz;
	if (bin.samples < fftLen) {
		if (bin.data) {
			fftwf_free(bin.data);
			bin.data = 0;
		}
		bin.samples = fftLen;
	}
	if (!bin.data) {
		size_t size = bin.samples * sizeof(ComplexFloat32);
		bin.data = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
		Assert(bin.data);
	}
	float64_t dfHz = MHZ_TO_HZ(fMHz - freqMHz);
	int32_t b = lrint(dfHz / wHz);
	int32_t spectra = ac.samples / fftLen;
	int32_t ofs = spectrum * fftLen;;
	if (overlap) {
		spectra *= 2;
		ofs /= 2;
	}
	if (bin.fftLen != fftLen) {
		if (bin.plan) {
			fftwf_destroy_plan(bin.plan);
			bin.plan = 0;
		}
		bin.fftLen = fftLen;
		createPlan(bin.plan, bin.data, bin.fftLen,  FFTW_FORWARD);
	}
	fftwf_execute_dft(bin.plan, (fftwf_complex *) ac.data + ofs,
			(fftwf_complex *) bin.data);
	rescale(bin.data, bin.fftLen, bin.fftLen);
	ComplexFloat32 val;
	if (b < 0)
		b += bin.fftLen;
	val = bin.data[b];
	// find the largest bin in the spectrum
	ComplexFloat32 largest(0, 0);
	int32_t lBin = 0;
	float32_t lPower = 0;
	ComplexFloat32 v;
	for (int32_t i = 0; i < bin.fftLen; ++i) {
		v = bin.data[i];
		if (norm(v) > lPower) {
			lPower = norm(v);
			largest = v;
			lBin = i;
		}
	}
#ifdef notdef
	if (lPower > norm(val)) {
		std::cout << "spectrum " << spectrum << ", bin " << b;
		std::cout << ", power " << norm(val);
		std::cout << ", largest " << lBin << ", power " << lPower << std::endl;
	}
#endif
	return (val);
}
/**
 * Create a set of spectra from a signal channel.
 */
void
ArchiveChannel::createSignalSpectra(const ComplexFloat32 *tdData,
		ComplexFloat32 *fdData, int32_t fftLen, int32_t samples, bool overlap)
{
	Assert(fdData);
	Assert(!(fftLen % 2));
	ss.spectra = getSignalSpectra(fftLen, samples, overlap);
	ss.stride = fftLen;
	if (overlap)
		ss.stride /= 2;

	if (ss.samples < fftLen) {
		if (ss.data) {
			fftwf_free(ss.data);
			ss.data = 0;
		}
		ss.samples = fftLen;
	}
	if (!ss.data) {
		size_t size = ss.samples * sizeof(ComplexFloat32);
		ss.data = static_cast<ComplexFloat32 *> (fftwf_malloc(size));
		Assert(ss.data);
	}
	if (ss.fftLen != fftLen) {
		if (ss.plan) {
			fftwf_destroy_plan(ss.plan);
			ss.plan = 0;
		}
		ss.fftLen = fftLen;
		createPlan(ss.plan, ss.data, fftLen, FFTW_FORWARD);
	}
	int32_t half = ss.fftLen / 2;
	for (int32_t i = 0; i < ss.spectra; ++i) {
		ComplexFloat32 *fd = fdData + i * fftLen;
		fftwf_execute_dft(ss.plan, (fftwf_complex *) tdData + i * ss.stride,
				(fftwf_complex *) ss.data);
		for (int32_t j = 0; j < ss.fftLen; ++j) {
			if (j < half)
				fd[j+half] = ss.data[j];
			else
				fd[j-half] = ss.data[j];
		}
		rescale(fd, ss.fftLen, ss.fftLen);
	}
}

/**
 * Dedrift and heterodyne the channel to produce a signal at DC and zero drift.
 */
void
ArchiveChannel::dedrift(ComplexFloat32 *iData, ComplexFloat32 *oData,
		float64_t fMHz, float64_t driftHz)
{
	float96_t dTheta = -2 * M_PI * (fMHz - freqMHz) / ac.widthMHz;
	float96_t widthHz = MHZ_TO_HZ(ac.widthMHz);
	float96_t d2Theta = -M_PI * driftHz / (widthHz * widthHz);
	ComplexFloat96 vector(1, 0);
	ComplexFloat96 omega(cosl(dTheta + d2Theta), sinl(dTheta + d2Theta));
	ComplexFloat96 dOmega(cosl(2 * d2Theta), sinl(2 * d2Theta));

	for (int32_t i = 0; i < ac.samples; ++i) {
		ComplexFloat96 data = iData[i];
		oData[i] = data * vector;
		vector *= omega;
		omega *= dOmega;
	}
}

}
