/*******************************************************************************

 File:    ATAPacket.h
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
// Class definition for ATA packet base class
//
#ifndef	ATAPACKET_H_
#define	ATAPACKET_H_

#include <complex>
#include "ATADataPacket.h"

using std::complex;

/**
 * ATA packet class.
 *
 * Description:\n
 * 	This is a wrapper class for the data packet proper; it is an
 * 	abstract class of which BeamPacket and ChannelPacket are subclasses.\n\n
 * Notes:\n
 * 	Since this class contains virtual functions, it cannot serve as the
 * 	primary packet class, which must be transmitted over the network,
 * 	because it contains a vtable for the virtual functions.  So, it
 *	encapsulates an ATADataPacket, defined in ATADataPacket.h.\n
 *	Use of a base class and subclasses allows us to use different packet
 *	formats for beam and channel without major ramifications in the
 *	application code.
 */
class ATAPacket
{
public:
	ATAPacket(uint32_t grp = ATADataPacketHeader::UNDEFINED,
		uint32_t src = ATADataPacketHeader::UNDEFINED,
		uint32_t channel = ATADataPacketHeader::UNDEFINED,
		uint32_t len = ATADataPacketHeader::DEFAULT_LEN,
		uint8_t bitsPerSample = ATADataPacketHeader::DEFAULT_BITS)
	{
	}

protected:
	virtual void flipEndian() = 0;

	/**
	 * Convert a floating point value to saturated integer.
	 *
	 * Description:\n
	 * 	Converts the floating point value to a saturated integer; if the
	 * 	input value is greater than the maximum positive integer the return
	 * 	value is set to the maximum; similarly, if it smaller than the
	 * 	minimum negative integer the return value is set to the minimum\n\n
	 * Notes:\n
	 * 	This default is for 16-bit integers.
	 */
	virtual int32_t saturate(float f) {
		if (fabs(f) > 32767)
			f = (f > 0 ? 32767 : -32767);
		return (lrintf(f));
	}

public:
	virtual void *getSamples() = 0;
	virtual complex<float> getSample(int32_t i) = 0;
	virtual void getSamples(complex<float> *data) = 0;
	virtual void putSample(int32_t i, const complex<float>& v) = 0;
	virtual void putSamples(const complex<float> *data) = 0;
	virtual int32_t getDataSize() = 0;
	virtual int32_t getPacketSize() = 0;
	virtual int32_t getSize() = 0;
	virtual int32_t getLen() = 0;
	virtual ATADataPacketHeader& getHeader() = 0;
	virtual void putHeader(const ATADataPacketHeader& hdr) = 0;
	virtual void *getData() = 0;
	virtual ATADataPacket *getPacket() = 0;
	virtual void getPacket(ATADataPacket *packet) = 0;
	virtual void putPacket(const ATADataPacket *packet) = 0;
	virtual bool marshall(ATADataPacketHeader::MarshallMode mode
			= ATADataPacketHeader::LAZY) = 0;
	virtual bool demarshall() = 0;

};

#endif // ATAPACKET_H
