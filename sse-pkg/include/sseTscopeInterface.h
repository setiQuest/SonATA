/*******************************************************************************

 File:    sseTscopeInterface.h
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


#ifndef SSE_TSCOPE_INTERFACE_H
#define SSE_TSCOPE_INTERFACE_H

// forward declare ostream
#include <iosfwd>  

#include <config.h>
#include <machine-dependent.h>
#include <sseInterface.h>
#include "SseMsg.h"

const char *const SSE_TSCOPE_INTERFACE_VERSION = "$Revision: 1.45 $ $Date: 2009/04/08 21:40:23 $";
typedef char8_t sseTscopeInterfaceVersionNumber[MAX_TEXT_STRING];

enum TscopeMessageCode
{
   TSCOPE_MSG_UNINIT = TSCOPE_CODE_RANGE_START,

   // --- outgoing commands to tscope
   TSCOPE_ALLOCATE,
   TSCOPE_DEALLOCATE,
   TSCOPE_MONITOR,
   TSCOPE_POINT_SUBARRAY,
   TSCOPE_REQUEST_INTRINSICS,
   TSCOPE_RESET,
   TSCOPE_TUNE,
   TSCOPE_SHUTDOWN,
   TSCOPE_REQUEST_STATUS,
   TSCOPE_STOP,
   TSCOPE_STOW,
   TSCOPE_WRAP,
   TSCOPE_CONNECT,
   TSCOPE_DISCONNECT,
   TSCOPE_SIMULATE,
   TSCOPE_UNSIMULATE,
   TSCOPE_ZFOCUS,
   TSCOPE_LNA_ON,
   TSCOPE_PAM_SET,
   TSCOPE_REQUEST_POINT_CHECK,
   TSCOPE_SEND_BACKEND_CMD,
   TSCOPE_ANTGROUP_AUTOSELECT,
   TSCOPE_BEGIN_SENDING_COMMAND_SEQUENCE,
   TSCOPE_DONE_SENDING_COMMAND_SEQUENCE,

   // beamformer commands
   TSCOPE_BF_RESET,
   TSCOPE_BF_INIT,
   TSCOPE_BF_AUTOATTEN,
   TSCOPE_BF_SET_ANTS,
   TSCOPE_BF_CLEAR_ANTS,
   TSCOPE_BF_SET_ATTN,
   TSCOPE_BF_SET_OBSLEN,
   TSCOPE_BF_SET_COORDS,
   TSCOPE_BF_CLEAR_COORDS,
   TSCOPE_BF_SET_NULL_TYPE,
   TSCOPE_BF_ADD_NULL,
   TSCOPE_BF_CLEAR_NULLS,
   TSCOPE_BF_CAL,
   TSCOPE_BF_POINT,
   TSCOPE_BF_STOP,
   TSCOPE_BF_DEST, //JR - Added to support bfinit destinations

   // -- incoming messages from tscope
   TSCOPE_ERROR,
   TSCOPE_MESSAGE,
   TSCOPE_INTRINSICS,
   TSCOPE_TRACKING_ON,
   TSCOPE_TRACKING_OFF,
   TSCOPE_STATUS_MULTIBEAM,
   TSCOPE_READY,
   TSCOPE_CANCEL_TIMER,

   TSCOPE_MESSAGE_CODE_END
};

struct TscopeIntrinsics
{
   sseTscopeInterfaceVersionNumber interfaceVersionNumber;
   char8_t name[MAX_TEXT_STRING];

   TscopeIntrinsics();
   void marshall();
   void demarshall();

   friend ostream& operator << (ostream &strm, 
				const TscopeIntrinsics &intrinsics);
};

const int TSCOPE_MAX_NAME = 32;

enum TscopeBeam
{
   TSCOPE_INVALID_BEAM = -1,

   TSCOPE_BEAMXA1,
   TSCOPE_BEAMYA1,
   TSCOPE_BEAMXA2,
   TSCOPE_BEAMYA2,
   TSCOPE_BEAMXA3,
   TSCOPE_BEAMYA3,
   TSCOPE_BEAMXA4,
   TSCOPE_BEAMYA4,

   TSCOPE_BEAMXB1,
   TSCOPE_BEAMYB1,
   TSCOPE_BEAMXB2,
   TSCOPE_BEAMYB2,
   TSCOPE_BEAMXB3,
   TSCOPE_BEAMYB3,
   TSCOPE_BEAMXB4,
   TSCOPE_BEAMYB4,

   TSCOPE_BEAMXC1,
   TSCOPE_BEAMYC1,
   TSCOPE_BEAMXC2,
   TSCOPE_BEAMYC2,
   TSCOPE_BEAMXC3,
   TSCOPE_BEAMYC3,
   TSCOPE_BEAMXC4,
   TSCOPE_BEAMYC4,

   TSCOPE_BEAMXD1,
   TSCOPE_BEAMYD1,
   TSCOPE_BEAMXD2,
   TSCOPE_BEAMYD2,
   TSCOPE_BEAMXD3,
   TSCOPE_BEAMYD3,
   TSCOPE_BEAMXD4,
   TSCOPE_BEAMYD4,

   TSCOPE_N_BEAMS
};

enum TscopeTuning
{
   TSCOPE_INVALID_TUNING = -1,

   TSCOPE_TUNINGA,
   TSCOPE_TUNINGB,
   TSCOPE_TUNINGC,
   TSCOPE_TUNINGD,

   TSCOPE_N_TUNINGS
};

struct TscopePointing
{
   enum CoordSys
   {
      AZEL,
      J2000,
      GAL,
      UNINIT
   };

   CoordSys coordSys;
   int32_t alignPad; // aligment padding for marshalling
   double azDeg; 
   double elDeg;
   double raHours; 
   double decDeg;  
   double galLongDeg;  
   double galLatDeg; 

   TscopePointing();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm, 
				const TscopePointing& pointing);
};

struct TscopeTuningStatus
{
   double skyFreqMhz;

   TscopeTuningStatus();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm, 
				const TscopeTuningStatus& status);
};

struct TscopeIfChainStatus
{
   double skyFreqMhz;

   TscopeIfChainStatus();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm, 
				const TscopeIfChainStatus& status);
};

struct TscopeSubarrayStatus
{
   int32_t numTotal;
   int32_t numSharedPointing;
   int32_t numTrack;
   int32_t numSlew;
   int32_t numStop;
   int32_t numOffline;
   int32_t numDriveError;
   int32_t wrap;
   double zfocusMhz;
   double gcErrorDeg;   // great circle error

   TscopeSubarrayStatus();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm, 
				const TscopeSubarrayStatus& status);
};

/*
ATA tscope status. All entries are indexed by the associated
synthesized beam.
*/
struct TscopeStatusMultibeam
{
   char time[MAX_TEXT_STRING];  
   TscopeSubarrayStatus subarray[TSCOPE_N_BEAMS];
   TscopePointing primaryPointing[TSCOPE_N_BEAMS];
   TscopePointing synthPointing[TSCOPE_N_BEAMS];
   TscopeIfChainStatus ifChain[TSCOPE_N_BEAMS];
   TscopeTuningStatus tuning[TSCOPE_N_TUNINGS];
   char ataBackendHost[MAX_TEXT_STRING];
   bool_t simulated;
   int32_t alignPad;  // alignment padding for marshalling

   TscopeStatusMultibeam();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm, 
				const TscopeStatusMultibeam& status);
};

struct TscopeBeamCoords
{
   TscopeBeam beam;
   int32_t alignPad; // aligment padding for marshalling
   TscopePointing pointing;

   TscopeBeamCoords();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeBeamCoords& coords);
};


struct TscopeNullType
{
   enum NullType
   {
      AXIAL,
      PROJECTION,
      NONE,
      UNINIT
   };

   NullType nullType;
   int32_t alignPad;  // alignment padding for marshalling

   TscopeNullType();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeNullType& nullType);
};

struct TscopeSubarrayCoords
{
   char subarray[MAX_TEXT_STRING];
   TscopePointing pointing;

   TscopeSubarrayCoords();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeSubarrayCoords& coords);
};

/*
Define the antennas that make up a synthesized beam
*/
struct TscopeAssignSubarray
{
   TscopeBeam beam;
   char subarray[MAX_TEXT_STRING];  

   TscopeAssignSubarray();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeAssignSubarray& assign);
};

struct TscopeTuneRequest
{
   TscopeTuning tuning;
   int32_t alignPad;  // alignment padding for marshalling
   double skyFreqMhz; 

   TscopeTuneRequest();
   void marshall();
   void demarshall(); 

   friend ostream& operator << (ostream& strm, 
				const TscopeTuneRequest& req);
};

struct TscopeCalRequest
{
   enum CalType
   {
      DELAY,
      PHASE,
      FREQ,
      UNINIT
   };

   CalType calType;
   int32_t integrateSecs;
   int32_t numCycles;
   int32_t calIterations;
   int32_t alignPad;  // alignment padding for marshalling

   TscopeCalRequest();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeCalRequest& req);
};

struct TscopeStopRequest
{
   char subarray[MAX_TEXT_STRING];  

   TscopeStopRequest();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeStopRequest& req);
};

struct TscopeStowRequest
{
   char subarray[MAX_TEXT_STRING];  

   TscopeStowRequest();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeStowRequest& req);
};


struct TscopeZfocusRequest
{
   double skyFreqMhz; 
   char subarray[MAX_TEXT_STRING];  

   TscopeZfocusRequest();
   void marshall();
   void demarshall(); 

   friend ostream& operator << (ostream& strm, 
				const TscopeZfocusRequest& req);
};


struct TscopeWrapRequest
{
   int wrapNumber;
   char subarray[MAX_TEXT_STRING];  

   TscopeWrapRequest();
   void marshall();
   void demarshall();

   friend ostream& operator << (ostream& strm, 
				const TscopeWrapRequest& req);
};

struct TscopeMonitorRequest
{
   int periodSecs;
   int32_t alignPad;  // alignment padding for marshalling

   TscopeMonitorRequest();
   void marshall();
   void demarshall();

   friend ostream& operator << (ostream& strm, 
				const TscopeMonitorRequest& req);
};

struct TscopeLnaOnRequest
{
   char subarray[MAX_TEXT_STRING];  

   TscopeLnaOnRequest();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeLnaOnRequest& req);
};

struct TscopePamSetRequest
{
   char subarray[MAX_TEXT_STRING];  

   TscopePamSetRequest();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopePamSetRequest& req);
};

struct TscopeSubarray
{
   char subarray[MAX_TEXT_STRING];  

   TscopeSubarray();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeSubarray& sub);
};

struct TscopeBackendCmd
{
   char cmdWithArgs[MAX_TEXT_STRING];  

   TscopeBackendCmd();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm,
				const TscopeBackendCmd& cmd);
};

struct TscopeAntgroupAutoselect
{
   //int maxSefdJy;
   char bflist[MAX_TEXT_STRING];  
   //int32_t alignPad; // aligment padding for marshalling
   //double obsFreqMhz;

   TscopeAntgroupAutoselect();
   void marshall();
   void demarshall();

   friend ostream& operator << (ostream& strm, 
				const TscopeAntgroupAutoselect& antAuto);
};


#endif


