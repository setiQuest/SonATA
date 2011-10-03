/*******************************************************************************

 File:    packetread.cpp
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
** packetread.cpp 
**
*/

#include <unistd.h>	
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>

#include "basics.h"
#include "ChannelDataPacket.h"
#include "SseTimer.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;

#define CD_NUMBER_OF_CHANNELS                   (256)
#define MAX_POLS				(2)

string MULTICAST_ADDR("226.1.50.1");
int MULTICAST_PORT = 50000;
bool write2Disk = false;
bool writeX2Disk = false;
bool writeY2Disk = false;
bool printHdr = false;

const char *pgm = "packetread";

int packetCount = 0;
int seqGaps = 0;
ofstream fout;
ofstream Xfout;
ofstream Yfout;
ofstream debug;
int totalMissedPackets =0;
double seconds;
double elapsedSeconds;
double firstPacketSeconds;
int seq[CD_NUMBER_OF_CHANNELS][MAX_POLS];
int pktCount[CD_NUMBER_OF_CHANNELS][MAX_POLS];
int maxWait = 32;
char outFile[120];
char XoutFile[120];
char YoutFile[120];
bool dataOnly = false;
int maxChanRecv = 1;

//---------------------------------------------------------------------
// command line argument handling
//---------------------------------------------------------------------
void
usage()
{
	OUTL("usage: " << pgm <<
		 "\t[-a IP Address x.x.x.x (" << MULTICAST_ADDR << ")]\n"
		<<   "\t\t\t[-P Port Number (" << MULTICAST_PORT << ")]\n"
		<< "\t\t\t[-s secondsToWait (" << maxWait << ")] \n"  
                 << "\t\t\t[-w XYoutfile (false)] \n"
		<< "\t\t\t[-d Write Data Only, no Headers (false)  ]\n "
                 << "\t\t\t[-X XonlyOutfile (false)]\n"
		<< "\t\t\t[-Y YonlyOutfile (false)]\n"
		<< "\t\t\t[-p Print first Header]\n");
	exit(-1);
}

void
parseArgs(int argc, char **argv)
{
	const char *optstring = "pw:s:d:X:Y:P:a:";		// getopt options arguments 
	extern char *optarg;			// getopt argument value return string
	extern int	optind;				// current argv index
	bool done = false;				// getopt option parsing state
	extern int optopt;
	extern int opterr;
	opterr = 0;
	while ( !done )
    {
        switch ( getopt(argc,argv,optstring) )
        {
            case -1:
                done = true;
                break;

            case 'a':
                MULTICAST_ADDR = string(optarg);
                break;

            case 'P':
                MULTICAST_PORT = atoi(optarg);
                break;

            case 'w':
                write2Disk = true;
                strcpy(outFile, optarg);
                break;

            case 'X':
                writeX2Disk = true;
                strcpy(XoutFile, optarg);
                break;

            case 'Y':
                writeY2Disk = true;
                strcpy(YoutFile, optarg);
                break;

            case 's':
                maxWait = atoi(optarg);
                break;

            case 'd':
                dataOnly = true;
                break;

            case 'p':
                printHdr = true;
                break;

            default:
            case '?':
                USAGE("Invalid option: " << optopt );
        }
    }

	if ( optind < argc )
		USAGE("Too many arguments");
}

//---------------------------------------------------------------------
// in-line class under test
//---------------------------------------------------------------------
class MCRecv
{
public:
	MCRecv(int);
	int recv(void *,size_t);
private:
	int sockFd;
};

MCRecv::MCRecv(int channel)
{
    struct hostent *host;

	sockFd = socket(PF_INET, SOCK_DGRAM, 0);
	ASSERT(sockFd >= 0);

	int optval = 1;
        if (::setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, (char *) &optval,
                        sizeof(optval)) < 0) {
                ::close(sockFd);
        	DEBUG(ALWAYS, "Error on SO_REUSEADDR " );
		exit(-1);
        }
	int bufs = 16*1024 * 1024;
        socklen_t bufsize = sizeof(bufs);
        if (::setsockopt(sockFd, SOL_SOCKET, SO_RCVBUF, &bufs, bufsize) < 0) {
                ::close(sockFd);
        	DEBUG(ALWAYS, "Error on set SO_RCVBUF " );
		exit(-1);
	}
	socklen_t checkSize;
        if (::getsockopt(sockFd, SOL_SOCKET, SO_RCVBUF, &checkSize, &bufsize) < 0) {
                ::close(sockFd);
        	DEBUG(ALWAYS, "Error on get SO_RCVBUF " );
		exit(-1);
	}

	int result;

	uint16_t rcvPort = MULTICAST_PORT;
	host = gethostbyname(MULTICAST_ADDR.c_str());
	
	sockaddr_in myAddr;
	memset(&myAddr,0,sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_port = htons(rcvPort);
	// Let host assign IP address; we just want to bind the port
	// number.  Note that the memset to 0 is equivalent to
	// myAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	result = bind(sockFd, (sockaddr *)&myAddr, sizeof(myAddr));
	ASSERT(result == 0);

	ip_mreq mreq;
	memset(&mreq,0,sizeof(mreq));
	memcpy(&mreq.imr_multiaddr.s_addr, (host->h_addr), host->h_length);

	result = setsockopt(sockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			&mreq, sizeof(mreq));
	ASSERT(result >= 0);

}

int
MCRecv::recv(void *buf,size_t size)
{
	sockaddr_in fromAddr;
	socklen_t addrLen = sizeof(sockaddr_in);

	int result = recvfrom(sockFd, buf, size, 0,
			(sockaddr *)&fromAddr, &addrLen);
	return result;
}

//---------------------------------------------------------------------
// main
//---------------------------------------------------------------------
#include <signal.h>
void
alarmHandler(int)
{
	cout << "MCAddress "<< MULTICAST_ADDR <<" MCPort "<< MULTICAST_PORT << endl;
	cout << "Got " << packetCount << " packets; "
		<< seqGaps << " gap(s) in sequence; " << totalMissedPackets
		<< " Total Missed Packets" << endl;
        elapsedSeconds = seconds - firstPacketSeconds;
        cout << "Total Elapsed Time for Transmission = " <<
                   elapsedSeconds << endl;
        cout << "Packets length = " << sizeof(ChannelDataPacket) <<
                   " bytes"  << endl;
        cout << "Samples/Packet = " << ATADataPacketHeader::DEFAULT_LEN << endl;
        cout << "Gbit/s = " << 
		sizeof(ChannelDataPacket)*packetCount*8/1.e9/elapsedSeconds 
		<< endl;

        cout << "kPackets/s = " << 
                packetCount/(elapsedSeconds)/1000<< endl;
        cout << "kSamples/s = " << 
packetCount*ATADataPacketHeader::DEFAULT_LEN/(elapsedSeconds)/1000 << endl;
	for ( int q = 0; q <= maxChanRecv; q++){
           cout << "Packets received for channel " << q <<
               " X pol " << pktCount[q][0] << " Y Pol " << pktCount[q][1] << endl;
	}
	if (write2Disk) fout.close();
	if (writeX2Disk) Xfout.close();
	if (writeY2Disk) Yfout.close();
	debug.close();
	exit(1);
}

int
main(int argc, char **argv)
{
	int exp[CD_NUMBER_OF_CHANNELS][MAX_POLS];
        int dataSize;
        int pktSize;

	parseArgs(argc,argv);

        if (write2Disk )fout.open(outFile, (ios::app | ios:: binary));
        if (writeX2Disk )Xfout.open(XoutFile, (ios::app | ios:: binary));
        if (writeY2Disk )Yfout.open(YoutFile, (ios::app | ios:: binary));
	debug.open("debugData");
	signal(SIGALRM, alarmHandler);
	cout << "Waiting " << maxWait << " seconds" << endl;
	alarm(maxWait);

	int channel = 1;
	MCRecv rcv(channel);
	ChannelDataPacket pkt;
	SseTimer myTimer;
        dataSize = ATADataPacketHeader::DEFAULT_LEN*4;	
        pktSize = sizeof(pkt);


	for ( int q = 0; q < CD_NUMBER_OF_CHANNELS; q++){
        exp[q][0] = exp[q][1] = 0;
		seq[q][0] = seq[q][1] = 0;
            pktCount[q][0] = pktCount[q][1] = 0;
	}
	
	myTimer.start();
	for(;;)
	{
	    int chan;
		int pol;

		rcv.recv(&pkt,pktSize);
                if ( packetCount == 0 )
		{
                    firstPacketSeconds = myTimer.total_elapsed_time();
			pkt.hdr.marshall();
#ifdef notdef
			for ( int q = 0; q < CD_NUMBER_OF_CHANNELS; q++)
        			exp[q][0] = exp[q][1] = pkt.hdr.seq;
#endif
			if (printHdr) pkt.hdr.printHeader();
		}
		else
		{
			pkt.hdr.marshall();
		}
		chan = pkt.hdr.chan;
                if ( chan > maxChanRecv ) maxChanRecv = chan;
		if (pkt.hdr.polCode == ATADataPacketHeader::XLINEAR) pol = 0;
		else if (pkt.hdr.polCode == ATADataPacketHeader::YLINEAR) pol = 1;
		else {
                    debug << "Invalid Polarization " <<
                             pkt.hdr.polCode << endl;
			pol = 1;
		}
		if (!seq[chan][pol] && !exp[chan][pol])
			exp[chan][pol] = pkt.hdr.seq;
		seq[chan][pol] = pkt.hdr.seq;
//		pkt.marshall(ATADataPacketHeader::FORCE_BIG_ENDIAN);
	    if (write2Disk) {
                if (dataOnly)
                   fout.write((char *)&pkt.data, dataSize);
                else
                   fout.write((char *)&pkt,pktSize);	
            }
	    if (writeX2Disk && pol == 0) {
                if (dataOnly)
                   Xfout.write((char *)&pkt.data, dataSize);
                else
                   Xfout.write((char *)&pkt,pktSize);	
            }
	    if (writeY2Disk && pol == 1) {
                if (dataOnly)
                   Yfout.write((char *)&pkt.data, dataSize);
                else
                   Yfout.write((char *)&pkt,pktSize);	
            }
                 pktCount[chan][pol]++;
		if ( seq[chan][pol] != exp[chan][pol] )
		{
			debug << "Chan " << chan
			      << " pol " << pol
			  << " Expected " << exp[chan][pol]
			  << " Got " << seq[chan][pol] 
			<< " packet count " << pktCount[chan][pol]
				<< endl;
		    totalMissedPackets += (seq[chan][pol]-exp[chan][pol]);
			seqGaps++;
		}
		exp[chan][pol] = seq[chan][pol] + 1;
		packetCount++;
                seconds = myTimer.total_elapsed_time();
	}

	return 0;
}
