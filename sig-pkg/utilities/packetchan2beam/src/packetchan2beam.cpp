/*******************************************************************************

 File:    packetchan2beam.cpp
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "ATADataPacketHeader.h"
#include "ChannelPacket.h"
#include "BeamPacket.h"


using namespace std;

extern char *optarg;
extern int optind, opterr;

#define MAX_POLS 2
ATADataPacketHeader beamHdr;
ATADataPacketHeader chanHdr;
BeamPacket beampkt;

ChannelPacket chanpkt1;
ChannelPacket chanpkt2;

uint32_t sequence_num = 1;

void convertchan2beam( ifstream& datastrm);
void usage();
void mylog(const char *msg);
void mylog(ostringstream& ossmsg);
int seq[MAX_POLS];
int pktCount[MAX_POLS];
int expected[MAX_POLS];
int packetCount = 0;
int seqGaps = 0;
int totalMissedPackets =0;


 
void short2char(signed char * outpkt, signed short * inpkt1,
		signed short * inpkt2, int count) {
      char temp;
      for (int i = 0; i < count; i++) {
	     if ( *inpkt1 > 127 ) temp = 127;
		else if ( *inpkt1 < -128) temp = -128;
		else temp = static_cast<signed char>(*inpkt1);
             *outpkt = temp;
		inpkt1++;
		outpkt++;
	}
      for (int i = 0; i < count; i++) {
	     if ( *inpkt2 > 127 ) temp = 127;
		else if ( *inpkt2 < -128) temp = -128;
		else temp = static_cast<signed char>(*inpkt2);
             *outpkt = temp;
		inpkt2++;
		outpkt++;
	}
}
int main(int argc, char **argv) {
  int c;
  static char optstring[] = "";  // no options for this version 
  ostringstream ossmsg;

  // prepare for inevitable options
  while ((c = getopt(argc, argv, optstring)) != -1) {
    switch (c) {
    }
  }   

  // test for arguments header file and at least one data file
  if ((argc - optind) < 1) {
    usage();
    exit(EXIT_FAILURE);
  }

//	Initialize packetcounters.
        expected[0] = expected[1] = 0;
                seq[0] = seq[1] = 0;
            pktCount[0] = pktCount[1] = 0;

  ifstream datastrm;

  for (int datafi = optind; datafi < argc; datafi++) {
    ostringstream dataopenoss;
    dataopenoss << "opening data file \"" << argv[datafi] << "\" ..."; 
    mylog(dataopenoss);
    // open first or next setiQuest data file
    datastrm.open(argv[datafi], ios::binary);
    if (!(datastrm)) {
      mylog("unable to open data file!");
      if (datafi > (optind + 1)) break;  // keep previously processed data 
      exit(EXIT_FAILURE);
    } 
    // Convert channel packets to beam packets
    // channelizer input compatible packets to stdout 
    convertchan2beam(datastrm);
    mylog("closing data file");
    datastrm.close();
  }
        ossmsg << "Packets: " << packetCount << endl
                << seqGaps << " gap(s) in sequence; " << totalMissedPackets
                << " Total Missed Packets" << endl;
	mylog(ossmsg);
  return (EXIT_SUCCESS);
}

void checkForMissingPackets( ATADataPacketHeader * hdr )
{
  ostringstream ossmsg;
  int pol;

  if (hdr->polCode == ATADataPacketHeader::XLINEAR) pol = 0;
        else if (hdr->polCode == ATADataPacketHeader::YLINEAR) pol = 1;
        else {
              ossmsg << "Invalid Polarization " << hdr->polCode << endl;
		mylog(ossmsg);
               pol = 1;
                }

   if (!seq[pol] && !expected[pol]) expected[pol] = hdr->seq;
   seq[pol] = hdr->seq;

   pktCount[pol]++;
   if ( seq[pol] != expected[pol] )
                {
                        ossmsg << " pol " << pol
                          << " Expected " << expected[pol]
                          << " Got " << seq[pol]
                        << " packet count " << pktCount[pol]
                                << endl;
			mylog(ossmsg);
                    totalMissedPackets += (seq[pol]-expected[pol]);
                        seqGaps++;
                }
    expected[pol] = seq[pol] + 1;
    packetCount++;
}

void convertchan2beam( ifstream& datastrm) {
  ostringstream ossmsg;

	int pktSize = chanpkt1.getDataSize() + sizeof(beamHdr);

  for (;;) {
    datastrm.read((char *) &chanpkt1, pktSize);  
    if (datastrm.gcount() != pktSize) {
      if (datastrm.eof()) return;  // EOF should occur on this read 
      ossmsg << "(1) unexpected data file read count " <<
        dec << datastrm.gcount();
      mylog(ossmsg);
      exit(EXIT_FAILURE);
    }
    chanpkt1.marshall();

// Copy the header into the output buffer

    memcpy( reinterpret_cast<void *>(&beamHdr), 
	reinterpret_cast<void *>(&chanpkt1), sizeof(beamHdr)) ;
    

	if (packetCount == 0)
	{
		expected[0] = expected[1] = beamHdr.seq;
	}

// Check for missing packets
	checkForMissingPackets(&beamHdr );
    datastrm.read((char *) &chanpkt2, pktSize);  
    if (datastrm.gcount() != pktSize) {
      ossmsg << "(2) unexpected data file read count " <<
        dec << datastrm.gcount();
      mylog(ossmsg);
      exit(EXIT_FAILURE);
    }
// Read the next packet

    chanpkt2.marshall();
    memcpy( reinterpret_cast<void *>(&chanHdr), 
	reinterpret_cast<void *>(&chanpkt2), sizeof(chanHdr)) ;
// Check for missing packets
	checkForMissingPackets( &chanHdr );
// Convert both packets' data to 8 bit complex and store in 
// output buffer.

    short2char(reinterpret_cast<signed char *>(beampkt.getData()),
		reinterpret_cast <signed short *>(chanpkt1.getData()),
		reinterpret_cast <signed short *>(chanpkt2.getData()),
      		chanpkt1.getDataSize());


    // sanity check for 0xaabbccdd endian order value
    if (beamHdr.order != ATADataPacketHeader::CORRECT_ENDIAN) {
      mylog("output packet header does not contain 0xaabbccdd endian value");
      exit(EXIT_FAILURE);
    } 
    // fix some of the fields to make packets look like they came
    // from the beamformer
    beamHdr.src = ATADataPacketHeader::BEAM_104MHZ;  // not really 
    beamHdr.chan = 1;
    beamHdr.seq = sequence_num++;
    beamHdr.len = 2048;
    memcpy( reinterpret_cast<void *>(&beampkt), 
		reinterpret_cast<void *>(&beamHdr), sizeof(beamHdr));
    // write the SonATA channelizer compatible packet to stdout
    cout.write((char *) &beampkt, pktSize); 
  }
}

void usage() {
  cerr << "usage: packetchan2beam [options] chandatafiles... ";
  cerr << "> beamPacketFile" << endl;
}

// should be in a separate file or class
void mylog(const char *msg) {
  ostringstream ossmsg(msg);
  mylog(ossmsg);
}

// should be in a separate file or class
void mylog(ostringstream& ossmsg) {
  timeval tv;
  time_t current_seconds;
  tm bdt;

  gettimeofday(&tv, NULL);
  current_seconds = tv.tv_sec;
  // not thread safe (but no threads used in this utility)
  memcpy(&bdt, gmtime(&current_seconds), sizeof(tm)); 
  
  cerr << setfill('0');
  cerr << dec << setw(4) << (bdt.tm_year + 1900) << "-" << setw(2) <<
    (bdt.tm_mon + 1) << "-" << setw(2) << bdt.tm_mday << "T" <<
    setw(2) << bdt.tm_hour << ":" << setw(2) << bdt.tm_min << ":" <<
    setw(2) << bdt.tm_sec << "." << setw(3) << (tv.tv_usec / 1000) <<      
    "Z" << " | " << ossmsg.str() << endl;
}
