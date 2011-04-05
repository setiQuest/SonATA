/*******************************************************************************

 File:    sseDxInterface.h
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

// sseDxInterface.h

#ifndef SSE_DX_INTERFACE_H
#define SSE_DX_INTERFACE_H

#include <config.h>
#include "machine-dependent.h"
#include "sseInterface.h"

// forward declare ostream
#include <iosfwd>  

const int MAX_RESOLUTIONS = 11;  // 1Hz - 1024 Hz
const int MAX_BASELINE_SUBCHANNELS = 8192;  // deprecated
const int MAX_SUBCHANNEL_BINS_PER_1HZ_HALF_FRAME = 1024;
const int MAX_SUBCHANNEL_BINS_PER_1KHZ_HALF_FRAME = 512;
const int MAX_DX_ACTIVITIES = 2;
const int MAX_CW_COHERENT_SEGMENTS = 8;

const char *const SSE_DX_INTERFACE_VERSION =
   "SSE-DX Interface Version 1.131 2011-Mar-30  20:01:48 UTC";

typedef char8_t sseDxInterfaceVersionNumber[MAX_TEXT_STRING];

enum Resolution { 
    RES_1HZ, RES_2HZ, RES_4HZ, RES_8HZ,
    RES_16HZ, RES_32HZ, RES_64HZ, RES_128HZ,
    RES_256HZ, RES_512HZ, RES_1KHZ, RES_UNINIT
};

enum SignalClass {  // signal classification
    CLASS_UNINIT,   // not yet assigned a classification 
    CLASS_CAND,     // candidate
    CLASS_RFI,      //
    CLASS_TEST,     // test signal
    CLASS_UNKNOWN   // everything else
};

// Signal Classification Reason
enum SignalClassReason {

    CLASS_REASON_UNINIT,    // not yet assigned a signal class reason

    //_____CLASS_CAND________

    // ** dx assigned: **
    PASSED_POWER_THRESH,
    PASSED_COHERENT_DETECT,	

    // ** sse assigned: **
    CONFIRM,
    RECONFIRM,
    NOT_SEEN_OFF,
    SECONDARY_FOUND_SIGNAL,

    SEEN_GRID_WEST,
    NOT_SEEN_GRID_WEST,

    SEEN_GRID_SOUTH,
    NOT_SEEN_GRID_SOUTH,

    SEEN_GRID_ON,
    NOT_SEEN_GRID_ON,

    SEEN_GRID_NORTH,
    NOT_SEEN_GRID_NORTH,

    SEEN_GRID_EAST,
    NOT_SEEN_GRID_EAST,

    GRID_PREDICTION,
    
    //___CLASS_RFI___________

    // ** dx assigned: **
    ZERO_DRIFT,
    RECENT_RFI_MATCH,
    FAILED_COHERENT_DETECT,
    FAILED_POWER_THRESH,
    NO_SIGNAL_FOUND,
    SNR_TOO_HIGH,
    SNR_TOO_LOW,
    DRIFT_TOO_HIGH,

    // ** sse assigned: **
    SEEN_OFF, 
    NO_RECONFIRM,
    SEEN_MULTIPLE_BEAMS,
    FALLS_IN_BAD_BAND,

    FAILED_COHERENT_DETECT_GRID_WEST,
    ZERO_DRIFT_GRID_WEST,
    RECENT_RFI_MATCH_GRID_WEST,

    FAILED_COHERENT_DETECT_GRID_SOUTH,
    ZERO_DRIFT_GRID_SOUTH,
    RECENT_RFI_MATCH_GRID_SOUTH,

    FAILED_COHERENT_DETECT_GRID_ON,
    ZERO_DRIFT_GRID_ON,
    RECENT_RFI_MATCH_GRID_ON,

    FAILED_COHERENT_DETECT_GRID_NORTH,
    ZERO_DRIFT_GRID_NORTH,
    RECENT_RFI_MATCH_GRID_NORTH,

    FAILED_COHERENT_DETECT_GRID_EAST,
    ZERO_DRIFT_GRID_EAST,
    RECENT_RFI_MATCH_GRID_EAST,

    
    //____ CLASS_TEST ___________

    // ** sse assigned: **
    TEST_SIGNAL_MATCH,
    
    //___ CLASS_UNKNOWN__________

    // ** dx assigned: **
    TOO_MANY_CANDIDATES,
    // PASSED_POWER_THRESH (already defined above)
    
    // ** sse assigned: **
    // SEEN_OFF (already defined above)
    BIRDIE_SCAN,
    RFI_SCAN,
    SECONDARY_NO_SIGNAL_FOUND,
    SIGNAL_NOT_IN_CHANNEL,

    SIGNAL_CLASS_REASON_END
};



// message codes (message type identifier)
enum DxMessageCode {
    MESSAGE_CODE_UNINIT = DX_CODE_RANGE_START,
    REQUEST_INTRINSICS,
    SEND_INTRINSICS,
    CONFIGURE_DX,
    PERM_RFI_MASK,
    BIRDIE_MASK,
    RCVR_BIRDIE_MASK,
    RECENT_RFI_MASK,
    TEST_SIGNAL_MASK,
    REQUEST_DX_STATUS,
    SEND_DX_STATUS,
    SEND_DX_ACTIVITY_PARAMETERS,
    DX_TUNED,
    DX_SCIENCE_DATA_REQUEST,
    START_TIME,
    BASELINE_INIT_ACCUM_STARTED,
    BASELINE_INIT_ACCUM_COMPLETE,
    DATA_COLLECTION_STARTED,
    DATA_COLLECTION_COMPLETE,
    SIGNAL_DETECTION_STARTED,
    SIGNAL_DETECTION_COMPLETE,
    BEGIN_SENDING_CANDIDATES,
    SEND_CANDIDATE_CW_POWER_SIGNAL,
    SEND_CANDIDATE_PULSE_SIGNAL,
    DONE_SENDING_CANDIDATES,
    BEGIN_SENDING_SIGNALS,
    SEND_CW_POWER_SIGNAL,
    SEND_PULSE_SIGNAL,
    DONE_SENDING_SIGNALS,
    BEGIN_SENDING_CW_COHERENT_SIGNALS,
    SEND_CW_COHERENT_SIGNAL,
    DONE_SENDING_CW_COHERENT_SIGNALS,
    BEGIN_SENDING_CANDIDATE_RESULTS,
    SEND_CW_COHERENT_CANDIDATE_RESULT,
    SEND_PULSE_CANDIDATE_RESULT,
    DONE_SENDING_CANDIDATE_RESULTS,
    BEGIN_SENDING_FOLLOW_UP_SIGNALS,
    SEND_FOLLOW_UP_CW_SIGNAL,
    SEND_FOLLOW_UP_PULSE_SIGNAL,
    DONE_SENDING_FOLLOW_UP_SIGNALS,
    REQUEST_ARCHIVE_DATA,
    DISCARD_ARCHIVE_DATA,
    ARCHIVE_SIGNAL,
    BEGIN_SENDING_ARCHIVE_COMPLEX_AMPLITUDES,
    SEND_ARCHIVE_COMPLEX_AMPLITUDES,
    DONE_SENDING_ARCHIVE_COMPLEX_AMPLITUDES,
    ARCHIVE_COMPLETE,
    SEND_DX_MESSAGE,
    SEND_BASELINE,
    SEND_COMPLEX_AMPLITUDES,
    STOP_DX_ACTIVITY,
    SHUTDOWN_DX,
    RESTART_DX,
    DX_ACTIVITY_COMPLETE,
    SEND_BASELINE_STATISTICS,
    BASELINE_WARNING_LIMITS_EXCEEDED,
    BASELINE_ERROR_LIMITS_EXCEEDED,
    BEGIN_SENDING_BAD_BANDS,
    SEND_PULSE_BAD_BAND,
    SEND_CW_BAD_BAND,
    DONE_SENDING_BAD_BANDS,
    DX_MESSAGE_CODE_END   // keep this as the last enum value
};


struct HereIAm
{
    sseDxInterfaceVersionNumber interfaceVersionNumber;

    HereIAm();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const HereIAm &hereIAm);
}; 

struct ThereYouAre
{
    IpAddress sseIp;  // sse address for dx to connect to
    int32_t portId;
    sseDxInterfaceVersionNumber interfaceVersionNumber; 

    ThereYouAre();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const ThereYouAre &thereYouAre);
};

/**
 * Multicast base address struct
 */
struct DxBaseAddr
{
   IpAddress addr;
   int32_t port;

   DxBaseAddr();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream& strm, const DxBaseAddr& baseAddr);
};

struct DxIntrinsics
{
    // public data
    sseDxInterfaceVersionNumber interfaceVersionNumber;
    char8_t name[MAX_TEXT_STRING];
    char8_t host[MAX_TEXT_STRING];
    char8_t codeVersion[MAX_TEXT_STRING];	// Version Number of DX Code

    // input
    DxBaseAddr channelBase;   // channel (input) base address

    // Digital filter bank (DFB)
    int32_t foldings;          // # of foldings
    float32_t oversampling;    // oversampling (as a percentage overlap)
    char8_t filterName[MAX_TEXT_STRING]; // filter name

    float64_t hzPerSubchannel;   // usable subchannel width
    int32_t maxSubchannels;      // Max # of subchannels (indicates DX bandwidth)
    int32_t serialNumber;	    
    NssDate birdieMaskDate;   // Time Stamp of Birdie Mask 
    NssDate rcvrBirdieMaskDate; // Time Stamp of Receiver Birdie Mask 
    NssDate permMaskDate;     // Time Stamp of Permanent RFI Mask 

    // methods
    DxIntrinsics();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DxIntrinsics &intrinsics);
};



struct DxConfiguration
{ 
    SiteId site;
    int32_t dxId;	      // Logical Id
    float64_t a2dClockrate;      // MHz
    char8_t archiverHostname[MAX_TEXT_STRING];
    int32_t archiverPort;
    int32_t alignPad;         // alignment padding for marshalling

    DxConfiguration();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DxConfiguration &config);
};



struct FrequencyBand
{
    float64_t centerFreq; // MHz
    float32_t bandwidth;  // MHz
    int32_t alignPad;     // alignment padding for marshalling

    FrequencyBand();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const FrequencyBand &fb);
};

// variable length structure
struct FrequencyMaskHeader
{
    int32_t numberOfFreqBands;
    NssDate maskVersionDate;
    int32_t alignPad;           // alignment padding for marshalling
    FrequencyBand bandCovered;	// overall band that the mask covers 

    FrequencyMaskHeader();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const FrequencyMaskHeader &hdr);
};
// FrequencyMaskHeader is followed by this array:
// FrequencyBand frequencyBandArray[numberOfFreqBands];


// variable length structure
struct RecentRfiMaskHeader
{
    int32_t numberOfFreqBands;	 
    int32_t excludedTargetId;	// Current Target signals excluded from list 
    FrequencyBand bandCovered;  // overall band that the mask covers 

    RecentRfiMaskHeader();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const RecentRfiMaskHeader &hdr);
};
// RecentRfiMaskHeader is followed by this array:
// FrequencyBand frequencyBandArray[numberOfFreqBands];


enum SciDataRequestType {REQ_FREQ, REQ_SUBCHANNEL};

struct DxScienceDataRequest
{
    bool_t sendBaselines;
    bool_t sendBaselineStatistics;
    bool_t checkBaselineWarningLimits;
    bool_t checkBaselineErrorLimits;
    int32_t baselineReportingHalfFrames; // # halfFrames between baselines
    bool_t sendComplexAmplitudes;
    SciDataRequestType requestType;  // request compAmps by freq or subchannel
    int32_t subchannel;     // request subchannel directly
    float64_t rfFreq;    // request subchannel containing this rf freq

    DxScienceDataRequest();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DxScienceDataRequest &dataRequest);
};

struct PulseParameters
{
    float64_t pulseThreshold;     // sigma
    float64_t tripletThreshold;   // sigma
    float64_t singletThreshold;   // sigma

    PulseParameters();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const PulseParameters &pp);
};


// Operations defined in DxActivityParameters operations field.
// Enum value represents the bit position in the mask.

enum DxOperationsMask
{
    DATA_COLLECTION, 
    BASELINING,
    FREQ_INVERSION,
    PULSE_DETECTION,  
    POWER_CWD, 
    COHERENT_CWD,
    APPLY_BIRDIE_MASK, 
    APPLY_RCVR_BIRDIE_MASK,
    APPLY_PERMANENT_RFI_MASK, 
    APPLY_RECENT_RFI_MASK, 
    APPLY_TEST_SIGNAL_MASK,
    APPLY_DOPPLER,   
    REJECT_ZERO_DRIFT_SIGNALS,
    CANDIDATE_SELECTION, 
    PROCESS_SECONDARY_CANDIDATES,
    FOLLOW_UP_CANDIDATES,
    SEND_RAW_SIGNAL_DETECTION_PRODUCTS, // cw paths - no clustering
    N_DX_OPERATIONS_MASK
};


// Used to determine when the baseline statistics are out of
// the expected limits.

struct BaselineLimits
{
    float32_t meanUpperBound;
    float32_t meanLowerBound;
    float32_t stdDevPercent;
    float32_t maxRange;

    BaselineLimits();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const BaselineLimits &limits);

};


// The values below marked "mode dependent" are expected to vary
// depending on the dx mode (ie, main, remote, main-only, etc.).

struct DxActivityParameters
{
    int32_t activityId;
    int32_t dataCollectionLength; // seconds
    float64_t rcvrSkyFreq;        // RF Freq (MHz) of Receiver band center
    float64_t ifcSkyFreq;         // RF Freq (MHz) of IFC band center
    float64_t dxSkyFreq;         // RF Freq (MHz) of DX band center
    int32_t channelNumber;        // channel number
    uint32_t operations;          // bit mask for DX operations
    float32_t sensitivityRatio;   // main/remote relative sensitivity
    int32_t maxNumberOfCandidates;
    float32_t clusteringFreqTolerance; // Hz
    float32_t zeroDriftTolerance;      // Hz/Sec
    float32_t maxDriftRateTolerance;      // Hz/Sec
    float32_t alignPad1;

    // CW
    float64_t badBandCwPathLimit;   // per KHz
    int32_t cwClusteringDeltaFreq;  // bins
    Resolution daddResolution;      // RES_1HZ, RES_2HZ, or RES_4HZ
    float64_t daddThreshold;	    // Sigma 
    float64_t cwCoherentThreshold;  // Sigma (mode dependent)
    float64_t secondaryCwCoherentThreshold; // Sigma 
    float64_t secondaryPfaMargin;    // Sigma
    float64_t limitsForCoherentDetection;

    // Pulse
    float64_t badBandPulseTripletLimit; // per KHz
    float64_t badBandPulseLimit;        // per KHz
    int32_t pulseClusteringDeltaFreq;   // bins
    float32_t pulseTrainSignifThresh;   // (mode dependent)
    float32_t secondaryPulseTrainSignifThresh; // Sigma
    int32_t maxPulsesPerHalfFrame;
    int32_t maxPulsesPerSubchannelPerHalfFrame;
    bool_t requestPulseResolution[MAX_RESOLUTIONS]; 
    PulseParameters pd[MAX_RESOLUTIONS]; 

    DxScienceDataRequest scienceDataRequest;

    // Number of subchannels over which to average for baseline statistics.
    // Should be an integer factor of the  total number of subchannels.
    int32_t baselineSubchannelAverage; 
                                       
    // The number of half-frames to accumulate baselines before
    // data collection begins.
    int32_t baselineInitAccumHalfFrames;

    float32_t baselineDecay;  // decay factor

    // baseline statistics monitoring limits
    BaselineLimits baselineWarningLimits;
    BaselineLimits baselineErrorLimits;

    int32_t alignPad2;               // alignment padding for marshalling

    DxActivityParameters();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DxActivityParameters &actParam);
};

struct DxTuned 
{
    float64_t dxSkyFreq;          // RF freq (MHz) of DX Band center
    int32_t dataCollectionLength;  // seconds
    int32_t dataCollectionFrames;  // collection length in frames

    DxTuned();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DxTuned &dxTuned);
};


struct StartActivity
{
    NssDate startTime;

    StartActivity();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const StartActivity &startAct);
};

// Signal Identifier.
// Must be unique for each signal.
struct SignalId
{
    int32_t dxNumber;   // number from dx hostname (eg, 7 for dx7)
    int32_t activityId;      
    NssDate activityStartTime;
    int32_t number;    // counter that starts at zero for each observation activity
    int alignPad1;     // aligment padding for marshalling

    SignalId();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const SignalId &signalId);
};

struct SignalPath
{
  float64_t rfFreq; // MHz
  float32_t drift;  // Hz/s
  float32_t width;  // Hz
  float32_t power;  
  int32_t alignPad; // alignment padding for marshalling

  SignalPath();
  void marshall();
  void demarshall();
  friend ostream& operator << (ostream &strm, const SignalPath &path);

};
        

struct SignalDescription
{
    SignalPath path;
    Polarization pol;
    SignalClass sigClass;  // signal classification
    SignalClassReason reason;   // reason for signal classification
    int32_t subchannelNumber; // subchannel containing this signal
    bool_t containsBadBands;
    SignalId signalId;     // uniq ID
    SignalId origSignalId; // original ID (for followup observations)
    int32_t alignPad; // alignment padding for marshalling

    SignalDescription();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const SignalDescription &sig);
};
 
struct CwPowerSignal
{
    SignalDescription sig;

    CwPowerSignal(); 
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const CwPowerSignal & cwps);
};

struct ConfirmationStats 
{
    float32_t pfa;            // prob. false alarm (e to -x)            
    float32_t snr;            // signal-to-noise in a 1Hz channel 

    ConfirmationStats();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const ConfirmationStats &cfm);
};

struct CwCoherentSegment
{
    SignalPath path;
    float32_t pfa;            // probability of false alarm
    float32_t snr;            // SNR in a 1Hz channel
    
    CwCoherentSegment();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const CwCoherentSegment & cwcs);
};


struct CwCoherentSignal
{
    SignalDescription sig;
    ConfirmationStats cfm;
    int32_t nSegments;
    int32_t alignPad;         // alignment padding for marshalling
    CwCoherentSegment segment[MAX_CW_COHERENT_SEGMENTS];

    CwCoherentSignal();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const CwCoherentSignal & cwcs);
};



// ----- begin Pulse Signal (variable length message) -------------------

struct Pulse
{
    float64_t rfFreq;      // MHz
    float32_t power;       // Janskys (?)
    int32_t alignPad;      // alignment padding for marshalling
    int32_t spectrumNumber;
    int32_t binNumber;
    Polarization pol;
    int32_t alignPad2;     // alignment padding for marshalling

    Pulse();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const Pulse &pulse);
};

// variable length
struct PulseTrainDescription
{
    float32_t pulsePeriod;   // seconds
    int32_t numberOfPulses;
    Resolution res;                // Resolution 
    int32_t alignPad;             // alignment padding for marshalling

    PulseTrainDescription();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const PulseTrainDescription &ptd);
};
// PulseTrainDescription is followed by this array:
// Pulse pulses[numberOfPulses];

struct PulseSignalHeader
{
    SignalDescription sig;
    ConfirmationStats cfm;
    PulseTrainDescription train;

    PulseSignalHeader();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const PulseSignalHeader &ps);
};

// ----- end Pulse Signal ------------------------

// ----- begin FollowUp Signal -------------------

struct FollowUpSignal
{
    float64_t rfFreq;       // projected signal freq at start of new obs (MHz)
    float32_t drift;        // Hz/sec
    Resolution res;         // 
    SignalId origSignalId;  // original signal Id

    FollowUpSignal();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const FollowUpSignal &fs);
};

struct FollowUpCwSignal
{
    FollowUpSignal sig;

    FollowUpCwSignal();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const FollowUpCwSignal &fcws);
};

struct FollowUpPulseSignal
{
    FollowUpSignal sig;

    FollowUpPulseSignal();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const FollowUpPulseSignal &fps);
};

// ----- end FollowUp Signal -------------------

struct DetectionStatistics 
{
    int32_t totalCandidates;  // total number of candidates (pulse + cw)
    int32_t cwCandidates;     
    int32_t pulseCandidates;
    int32_t candidatesOverMax; // candidates over the 'maxNumberOfCandidates' limit
    int32_t totalSignals;    // total number of signals (pulse + cw)
    int32_t cwSignals;
    int32_t pulseSignals;
    int32_t leftCwHits;      // # of left CW paths over threshold
    int32_t rightCwHits;     // # of right CW paths over threshold
    int32_t leftCwClusters;  // # of left CW clusters
    int32_t rightCwClusters; // # of left CW clusters
    int32_t totalPulses;     // # of combined pulses (=< leftPulses + rightPulses)
    int32_t leftPulses;      // # of left pol pulses
    int32_t rightPulses;     // # of right pol pulses
    int32_t triplets;        // # of triplets found
    int32_t pulseTrains;     // # of pulse trains found
    int32_t pulseClusters;   // # of pulse clusters
    
    DetectionStatistics();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DetectionStatistics &detStats);
};



struct CwBadBand
{
   FrequencyBand band; // frequency and width of bad band (MHz)
   Polarization pol;   // polarization
   int32_t paths;      // total number of paths over threshold
   int32_t maxPathCount;   // max number of paths allowed based on limit
   int32_t alignPad;   // alignment padding for marshalling
   SignalPath maxPath;

   CwBadBand();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream &strm, const CwBadBand &cwBadBand);
};

struct PulseBadBand
{
   FrequencyBand band; // frequency and width of bad band (MHz)
   Resolution res;     // resolution
   Polarization pol;   // polarization
   int32_t pulses;     // total pulses in band
   int32_t maxPulseCount;  // max number of pulses allowed based on limit
   int32_t triplets;   // total triplets in band
   int32_t maxTripletCount;  // max number of triplets allowed based on limit
   bool_t tooManyTriplets;
   int32_t alignPad;   // alignment padding for marshalling

   PulseBadBand();
   void marshall();
   void demarshall();
   friend ostream& operator << (ostream &strm, const PulseBadBand &pulseBadBand);
};



// ---- begin Baseline (variable length message) --------

struct BaselineValue
{
    float32_t value;
   
    BaselineValue();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const BaselineValue &bv);
};

struct BaselineHeader
{
    float64_t rfCenterFreq;       // MHz
    float64_t bandwidth;          // MHz
    int32_t  halfFrameNumber;		
    int32_t  numberOfSubchannels; 
    Polarization pol;
    int32_t activityId;

    BaselineHeader();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const BaselineHeader &hdr);
};
// BaselineHeader is followed by this array:
// BaselineValue values[numberOfSubchannels];


// This fixed length Baseline struct is deprecated.
// It's here for backwards compatibility.
struct Baseline
{
    BaselineHeader header;
    float32_t baselineValues[MAX_BASELINE_SUBCHANNELS];

    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const Baseline &baseline);
};

enum BaselineStatus
{
    BASELINE_STATUS_UNINIT,
    BASELINE_STATUS_GOOD,
    BASELINE_STATUS_WARNING,
    BASELINE_STATUS_ERROR
};

struct BaselineStatistics
{
    float32_t mean;
    float32_t stdDev;
    float32_t range;
    int32_t   halfFrameNumber;
    float64_t rfCenterFreqMhz;
    float64_t bandwidthMhz;
    Polarization pol;
    BaselineStatus status;
    
    BaselineStatistics();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const BaselineStatistics &stats);
};


struct BaselineLimitsExceededDetails
{
    Polarization pol;
    char description[MAX_NSS_MESSAGE_STRING]; 

    BaselineLimitsExceededDetails();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const BaselineLimitsExceededDetails &details);

};


// --- end Baseline ------


// ---- begin Complex Amplitudes (variable length message) -------

struct ComplexPair
{
    uint8_t pair;    // time samples: RRRRIIII real and imaginary.
                     // 4 bit signed (2's complement), high bits on the left.

    ComplexPair();
};

struct SubchannelCoef1KHz
{
    ComplexPair coef[MAX_SUBCHANNEL_BINS_PER_1KHZ_HALF_FRAME];

    SubchannelCoef1KHz();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const SubchannelCoef1KHz &subchannel);
};

// variable length
struct ComplexAmplitudeHeader
{
    float64_t rfCenterFreq;  // center freq of first subchannel (MHz)
    int32_t halfFrameNumber;
//  int32_t alignPad;               // replaced by activityId for sonata
    int32_t activityId;
    float64_t hzPerSubchannel;          
    int32_t startSubchannelId;
    int32_t numberOfSubchannels;
//  Resolution res;                 // deprecated, then replaced by overSampling
                                    // for sonata
    float32_t overSampling;         // % by which subchannel is oversampled
    Polarization pol;

    ComplexAmplitudeHeader();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const ComplexAmplitudeHeader &compAmpHdr);

};
// ComplexAmplitudeHeader is followed by this array:
// SubchannelCoef1KHz subchannelArray[numberOfSubchannels]



// This fixed length ComplexAmplitudes struct is deprecated.
// It's here for backwards compatibility.
struct ComplexAmplitudes
{
    ComplexAmplitudeHeader header;
    SubchannelCoef1KHz compamp;

    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const ComplexAmplitudes &compAmps);
};

// ---- end Complex Amplitudes -------



// ------ Archiving -------

// Request complex amplitude archive data for the given signal
struct ArchiveRequest
{
    SignalId signalId;

    ArchiveRequest();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const ArchiveRequest &archiveRequest);
};

// complex amplitude data response for an archive request
struct ArchiveDataHeader
{
    SignalId signalId;

    ArchiveDataHeader();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const ArchiveDataHeader &archiveDataHeader);
};
// ArchiveDataHeader is followed by this variable length array:
// SubchannelCoef1KHz subchannnelArray[numberOfSubchannels]

// ------ end Archiving -------


enum DxActivityState {
    DX_ACT_NONE,         // No activity
    DX_ACT_INIT,         // initialized
    DX_ACT_TUNED,        // tuned
    DX_ACT_PEND_BASE_ACCUM,  // pending initial baseline accumulation
    DX_ACT_RUN_BASE_ACCUM,   // running baseline accumulation
    DX_ACT_BASE_ACCUM_COMPLETE,  // end baseline accumulation
    DX_ACT_PEND_DC,      // pending data collection 
    DX_ACT_RUN_DC,       // running data collection
    DX_ACT_DC_COMPLETE,  // end data collection
    DX_ACT_PEND_SD,      // pending signal detection
    DX_ACT_RUN_SD,       // running signal detection
    DX_ACT_SD_COMPLETE,  // end signal detection
    DX_ACT_COMPLETE,     // activity complete
    DX_ACT_STOPPING,     // activity stopping
    DX_ACT_STOPPED,      // activity stopped
    DX_ACT_ERROR         // error condition
};

struct DxActivityStatus
{ 
    int32_t activityId;      
    DxActivityState currentState;   

    DxActivityStatus();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DxActivityStatus &actStatus);
};

struct DxStatus
{
    NssDate timestamp;
    int32_t numberOfActivities;
    int32_t alignPad;             // alignment padding for marshalling
    DxActivityStatus act[MAX_DX_ACTIVITIES];

    DxStatus();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const DxStatus &status);
};

struct Count
{
    int32_t count;  
    
    Count();
    void marshall();
    void demarshall();
    friend ostream& operator << (ostream &strm, const Count &count);
};


#endif
