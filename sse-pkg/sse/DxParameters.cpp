/*******************************************************************************

 File:    DxParameters.cpp
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


#include <ace/OS.h>            // to eliminate ACE errors
#include "DxParameters.h" 
#include "RangeParameter.h"
#include "ChoiceParameter.h"
#include "sseDxInterface.h"
#include "Site.h"
#include "machine-dependent.h" // for float64_t et. al.
#include "DxProxy.h"
#include "SseArchive.h"
#include <sstream>
#include "Assert.h"
#include "AtaInformation.h"
#include "ComponentControlImmedCmds.h"
extern ComponentControlImmedCmds componentControlImmedCmdsGlobal;

static const char *DbTableName="DxParameters";
static const char *IdColNameInActsTable="dxParametersId";
static const char *ChoiceOn = "on";
static const char *ChoiceOff = "off";
static const char *DataReqFreq = "freq";
static const char *DataReqSubchannel = "subchan";
static int MaxRecentRfiMaskSize = 4096;

const double DefaultDxSkyFreqMhz = AtaInformation::AtaDefaultSkyFreqMhz;

struct DxParametersInternal
{
   RangeParameter<int32_t> dataCollectionLengthSecs; 
   RangeParameter<int32_t> maxNumberOfCandidates;
   RangeParameter<float32_t> clusteringFreqTolerance;
   RangeParameter<float32_t> zeroDriftTolerance;
   RangeParameter<float32_t> maxDriftRateTolerance;

   // CW
   RangeParameter<float64_t> badBandCwPathLimit;
   RangeParameter<int32_t> cwClusteringDeltaFreq;
   ChoiceParameter<int32_t> daddResolution;
   RangeParameter<float64_t> daddThreshold;	

   RangeParameter<float64_t> cwCoherentThreshold;
   RangeParameter<float64_t> secondaryCwCoherentThreshold;
   RangeParameter<float64_t> secondaryPfaMargin;

   RangeParameter<float64_t> limitsForCoherentDetection;

   // Pulse
   ChoiceParameter<string> allowPulseDetection;
   RangeParameter<float64_t> badBandPulseTripletLimit;
   RangeParameter<float64_t> badBandPulseLimit;
   RangeParameter<int32_t> pulseClusteringDeltaFreq;
   RangeParameter<float32_t> pulseTrainSignifThresh;
   RangeParameter<float32_t> secondaryPulseTrainSignifThresh;
   RangeParameter<int32_t> maxPulsesPerHalfFrame;
   RangeParameter<int32_t> maxPulsesPerSubchannelPerHalfFrame;

   //bool_t requestPulseResolution[MAX_RESOLUTIONS]; 
   //PulseParameters pd[MAX_RESOLUTIONS]; 
   RangeParameter<float64_t> pulseThreshold;
   RangeParameter<float64_t> tripletThreshold;
   RangeParameter<float64_t> singletThreshold;
    
   RangeParameter<int32_t> baselineSubchannelAverage; 
   RangeParameter<int32_t> baselineInitAccumHalfFrames;

   // DxScienceDataRequest
   ChoiceParameter<string> sendBaselines;
   ChoiceParameter<string> sendBaselineStatistics;
   ChoiceParameter<string> checkBaselineWarningLimits;
   ChoiceParameter<string> checkBaselineErrorLimits;
   RangeParameter<int32_t> baselineReportingHalfFrames; // # halfFrames between baselines
   ChoiceParameter<string> sendComplexAmplitudes;
   ChoiceParameter<string> dataRequestType;
   RangeParameter<int32_t> dataRequestSubchannel;          // request subchannel directly
   RangeParameter<float64_t> dataRequestFreq;    
   RangeParameter<int32_t> dataRequestMaxCompampSubchannels;          // Max # of subchannels 

   // allow manual override of dx bandwidth
   ChoiceParameter<string> manualBandwidth;
   RangeParameter<int32_t> bandwidth;    

   RangeParameter<float32_t> baselineDecay;

   // baseline stats warning/error limits
   RangeParameter<float32_t> baselineWarningMeanUpperBound;
   RangeParameter<float32_t> baselineWarningMeanLowerBound;
   RangeParameter<float32_t> baselineWarningStdDevPercent;
   RangeParameter<float32_t> baselineWarningMaxRange;
    
   RangeParameter<float32_t> baselineErrorMeanUpperBound;
   RangeParameter<float32_t> baselineErrorMeanLowerBound;
   RangeParameter<float32_t> baselineErrorStdDevPercent;
   RangeParameter<float32_t> baselineErrorMaxRange;

   ChoiceParameter<string> recentRfiEnable;

   RangeParameter<float64_t> recentRfiMaskElementWidthMinHz;
   RangeParameter<int> recentRfiMaskSizeMax;

   DxActivityParameters dxActParam_;
   string outString_;

   // methods:
   DxParametersInternal();  
   const DxActivityParameters & getUpdatedDxActParam();
   void updateGeneralDxActParam(DxActivityParameters &ap);

   // default copy constructor and assignment operator are safe


};

// TBD don't hard code dxHzPerSubchannel since the value may change


const int dxHzPerSubchannel = 533.333;
const int defaultClusteringFreqTolerance = dxHzPerSubchannel / 2;

DxParametersInternal::DxParametersInternal() :

   dataCollectionLengthSecs(
      "length", "sec", "data collection length (13,24,48,94,195,388,774)",
      12, 1, 774),
   maxNumberOfCandidates(
      "maxcand", "count", "max number of candidates",
      8, 1, 1000),
   clusteringFreqTolerance(
      "clustfreqtol", "Hz", "clustering freq tolerance",
      defaultClusteringFreqTolerance, 0, 5000),   
   zeroDriftTolerance(
      "zerodrifttol", "Hz/Sec", "zero drift tolerance",
      0.007, 0.0, 1.0),
   maxDriftRateTolerance(
      "maxdrifttol", "Hz/Sec", "max driftrate tolerance",
      1.000, 0.0, 10.0),
 
   badBandCwPathLimit(
      "badbandcwpathlim", "", "bad band CW path limit per KHz",
      250, 0, 1e3),
   cwClusteringDeltaFreq(
      "cwclustdeltafreq", "bins", "cw clustering delta freq",
      2, 0, 1000),
   daddResolution(
      "daddres", "Hz", "dadd resolution",
      1),		
   daddThreshold(
      "daddthresh", "sigma", "dadd threshold",
      8.5, 1, 20),		
   cwCoherentThreshold(
      "cwthresh", "sigma", "cw coherent threshold",
      0, -100, 0),
   secondaryCwCoherentThreshold(
      "secondarycwthresh", "sigma", "secondary cw coherent threshold",
      -20, -100, 0),
   secondaryPfaMargin(
      "secondarypfamargin", "sigma", "secondary cw coherent pfa margin",
      3, 1, 10),

   limitsForCoherentDetection(
      "coherentdetlim", "", "limits for coherent detection",
      0, -1, 1),   // TBD set min/max to appropriate values

   allowPulseDetection(
      "pulsedetect",  "", 
      "allow pulse detection",
      ChoiceOn),
   badBandPulseTripletLimit(
      "badbandpulsetriplim", "", "bad band pulse triplet limit per KHz",
      5000, 0, 1e6),
   badBandPulseLimit(
      "badbandpulselim", "", "bad band pulse limit per KHz",
      300, 0, 1e5),
   pulseClusteringDeltaFreq(
      "pulseclustdeltafreq", "bins", "pulse clustering delta freq",
      25, 0, 1000),
   pulseTrainSignifThresh(
      "trainsignifthresh", "signif",
      "\n\t\tpulse train significance threshhold",
      -40, -100, 100),

   secondaryPulseTrainSignifThresh(
      "secondarytrainsignifthresh", "signif",
      "\n\t\tsecondary pulse train significance threshhold",
      -17, -100, 100),

   maxPulsesPerHalfFrame(
      "maxpulsesperhf", "# pulses", "max pulses per half frame",
      1000, 1, 10000),
   maxPulsesPerSubchannelPerHalfFrame(
      "maxpulsespersubperhf", "# pulses", "max pulses per subchannel per half frame",
      10, 1, 100),
   pulseThreshold(
      "pulsethresh", "sigma", "pulse threshold",
      12, 0, 100),
   tripletThreshold(
      "tripletthresh", "sigma", "triplet threshold",
      48, 0, 768), 
   singletThreshold(
      "singletthresh", "sigma", "singlet threshold",
      100, 0, 100), 
   baselineSubchannelAverage(
      "basesubave", "# subchannels",
      "# subchannels to average for baseline stats",
      1, 1, 1000), 
   baselineInitAccumHalfFrames(
      "baseinitaccum", "# half frames",
      "\n\t\t#half frames to accumulate initial baselines",
      20, 1, 20),

   // Science Data Request
   sendBaselines(
      "baselines", "", "enable baseline reporting",
      ChoiceOn),

   sendBaselineStatistics(
      "basestats",  "", 
      "report baseline statistics",
      ChoiceOn),

   checkBaselineWarningLimits(
      "basewarn",  "", 
      "report baseline warning limits exceeded",
      ChoiceOn),

   checkBaselineErrorLimits(
      "baseerror",  "", 
      "report baseline error limits exceeded",
      ChoiceOn),

   baselineReportingHalfFrames(
      "baserep", "# half frames",
      "baseline reporting interval",
      20, 1, 512),


   sendComplexAmplitudes(
      "compamps",  "", 
      "enable reporting of complex amplitudes",
      ChoiceOn),

   dataRequestType(
      "datareqtype", "", 
      "type of complex amplitudes request", 
      DataReqSubchannel),

   dataRequestSubchannel(
      "datareqsubchan", "subchannel #",
      "request subchannel by number",
      384, 0, 1535),

   dataRequestFreq(
      "datareqfreq", "MHz RF",
      "request subchannel containing freq",
      DefaultDxSkyFreqMhz + 0.800100, 0, 
      AtaInformation::AtaMaxSkyFreqMhz),

   dataRequestMaxCompampSubchannels(
      "datareqmaxcompampsubchan", "Max # of subchannels ",
      "Max # of subchannels",
      1, 0, 10),

   manualBandwidth(
      "manualbw",  "", 
      "set the assigned bandwidth manually",
      ChoiceOff),

   bandwidth(
      "bandwidth", "# subchannels",
      "\n\tassigned bandwidth (must be a multiple of 512)",
      2048, 512, 10240),

   baselineDecay(
      "basedecay", "",
      "baseline decay factor",
      0.9, 0.0, 1.0),

   // baseline warning

   baselineWarningMeanUpperBound(
      "basewarnmeanupper", "power",
      "baseline warning mean upper bound limit",
      3000, 0, 1e5),

   baselineWarningMeanLowerBound(
      "basewarnmeanlower", "power",
      "baseline warning mean lower bound limit",
      60, 0, 1e5),

   baselineWarningStdDevPercent(
      "basewarnstddev", "percent",
      "baseline warning std dev",
      50, 1, 100),

   baselineWarningMaxRange(
      "basewarnrange", "power",
      "baseline warning max range",
      1000, 0, 10e5),

   // baseline error

   baselineErrorMeanUpperBound(
      "baseerrormeanupper", "power",
      "baseline error mean upper bound limit",
      5000, 0, 1e5),

   baselineErrorMeanLowerBound(
      "baseerrormeanlower", "power",
      "baseline error mean lower bound limit",
      30, 0, 1e5),

   baselineErrorStdDevPercent(
      "baseerrorstddev", "percent",
      "baseline error std dev",
      80, 1, 100),

   baselineErrorMaxRange(
      "baseerrorrange", "power",
      "baseline error max range",
      3000, 0, 10e5),

   recentRfiEnable(
      "recentrfienable",  "", 
      "enable use of the recent RFI mask",
      ChoiceOn),

   recentRfiMaskElementWidthMinHz(
      "recentrfimaskelemwidmin", "Hz",
      "min width of a recent rfi mask element",
      1000, dxHzPerSubchannel, 10000),

   recentRfiMaskSizeMax(
      "recentrfimasksizemax", "count",
      "max number of elements in a recent rfi mask",
      MaxRecentRfiMaskSize, MaxRecentRfiMaskSize, MaxRecentRfiMaskSize)

{ 

}

const DxActivityParameters & DxParametersInternal::getUpdatedDxActParam()
{
   DxActivityParameters &ap = dxActParam_;

   // fill in the general fields
   updateGeneralDxActParam(ap);

   return ap;
}



void DxParametersInternal::updateGeneralDxActParam(DxActivityParameters &ap)
{
   // update the dxactparam struct from the
   // Parameters above

   ap.activityId = NSS_NO_ACTIVITY_ID;
   ap.dataCollectionLength = dataCollectionLengthSecs.getCurrent(); 
   ap.ifcSkyFreq = -999999;  // set in activity
   ap.dxSkyFreq = -999999; // set via parameter 'load' command

   ap.channelNumber=0;  // normally set by activity unless overridden
   // operations
   ap.operations = 0;   // no UI param

   ap.sensitivityRatio = 1;   // Single site only

   ap.maxNumberOfCandidates = maxNumberOfCandidates.getCurrent();
   ap.clusteringFreqTolerance = clusteringFreqTolerance.getCurrent();
   ap.zeroDriftTolerance = zeroDriftTolerance.getCurrent(); 
   ap.maxDriftRateTolerance = maxDriftRateTolerance.getCurrent(); 

   // CW
   ap.cwClusteringDeltaFreq = cwClusteringDeltaFreq.getCurrent();
   ap.badBandCwPathLimit = badBandCwPathLimit.getCurrent(); 

   switch (daddResolution.getCurrent())
   {
   case 1:
      ap.daddResolution = RES_1HZ;
      break;
   case 2:
      ap.daddResolution = RES_2HZ;
      break;
   case 4:
      ap.daddResolution = RES_4HZ;
      break;
   default:
      Assert(0);  // invalid dadd resolution
      break;
   }

   ap.daddThreshold = daddThreshold.getCurrent();             
   ap.cwCoherentThreshold = cwCoherentThreshold.getCurrent();
   ap.secondaryCwCoherentThreshold = secondaryCwCoherentThreshold.getCurrent();
   ap.secondaryPfaMargin = secondaryPfaMargin.getCurrent();

   ap.limitsForCoherentDetection = limitsForCoherentDetection.getCurrent();

   // Pulse
   ap.pulseClusteringDeltaFreq = pulseClusteringDeltaFreq.getCurrent();
   ap.badBandPulseTripletLimit = badBandPulseTripletLimit.getCurrent();
   ap.badBandPulseLimit = badBandPulseLimit.getCurrent();
   ap.pulseTrainSignifThresh = pulseTrainSignifThresh.getCurrent();
   ap.secondaryPulseTrainSignifThresh = secondaryPulseTrainSignifThresh.getCurrent();

   ap.maxPulsesPerHalfFrame =  maxPulsesPerHalfFrame.getCurrent();
   ap.maxPulsesPerSubchannelPerHalfFrame = maxPulsesPerSubchannelPerHalfFrame.getCurrent();

   for (int i=0; i< MAX_RESOLUTIONS; ++i)
   {
      ap.requestPulseResolution[i] = SSE_FALSE;  
      ap.pd[i].pulseThreshold = pulseThreshold.getCurrent(); 
      ap.pd[i].tripletThreshold = tripletThreshold.getCurrent();
      ap.pd[i].singletThreshold = singletThreshold.getCurrent();
   }
   // enable a few pulse resolutions
   ap.requestPulseResolution[RES_1HZ] = SSE_TRUE;  
   //ap.requestPulseResolution[RES_2HZ] = SSE_TRUE;  
   //ap.requestPulseResolution[RES_4HZ] = SSE_TRUE;  


   // Science Data Request
   ap.scienceDataRequest.sendBaselines = SSE_TRUE;  
   Assert(sendBaselines.isValid(ChoiceOff));
   if (sendBaselines.getCurrent() == ChoiceOff)
      ap.scienceDataRequest.sendBaselines = SSE_FALSE;  

   ap.scienceDataRequest.sendBaselineStatistics = SSE_TRUE;  
   Assert(sendBaselineStatistics.isValid(ChoiceOff));
   if (sendBaselineStatistics.getCurrent() == ChoiceOff)
      ap.scienceDataRequest.sendBaselineStatistics = SSE_FALSE;  

   ap.scienceDataRequest.checkBaselineWarningLimits = SSE_TRUE;  
   Assert(checkBaselineWarningLimits.isValid(ChoiceOff));
   if (checkBaselineWarningLimits.getCurrent() == ChoiceOff)
      ap.scienceDataRequest.checkBaselineWarningLimits = SSE_FALSE;  

   ap.scienceDataRequest.checkBaselineErrorLimits = SSE_TRUE;  
   Assert(checkBaselineErrorLimits.isValid(ChoiceOff));
   if (checkBaselineErrorLimits.getCurrent() == ChoiceOff)
      ap.scienceDataRequest.checkBaselineErrorLimits = SSE_FALSE;  

   ap.scienceDataRequest.baselineReportingHalfFrames =
      baselineReportingHalfFrames.getCurrent();

   ap.scienceDataRequest.sendComplexAmplitudes = SSE_TRUE;
   Assert(sendComplexAmplitudes.isValid(ChoiceOff));
   if (sendComplexAmplitudes.getCurrent() == ChoiceOff)
      ap.scienceDataRequest.sendComplexAmplitudes = SSE_FALSE;

   ap.scienceDataRequest.scienceData.requestType = REQ_FREQ;
   Assert(dataRequestType.isValid(DataReqSubchannel));
   if (dataRequestType.getCurrent() == DataReqSubchannel)
      ap.scienceDataRequest.scienceData.requestType = REQ_SUBCHANNEL;

   ap.scienceDataRequest.scienceData.subchannel
         = dataRequestSubchannel.getCurrent();
   ap.scienceDataRequest.scienceData.rfFreq = dataRequestFreq.getCurrent();

   ap.baselineSubchannelAverage = baselineSubchannelAverage.getCurrent(); 
   ap.baselineInitAccumHalfFrames = 
      baselineInitAccumHalfFrames.getCurrent();

   ap.baselineDecay = baselineDecay.getCurrent();

   ap.baselineWarningLimits.meanUpperBound =  baselineWarningMeanUpperBound.getCurrent();
   ap.baselineWarningLimits.meanLowerBound = baselineWarningMeanLowerBound.getCurrent();
   ap.baselineWarningLimits.stdDevPercent = baselineWarningStdDevPercent.getCurrent();
   ap.baselineWarningLimits.maxRange = baselineWarningMaxRange.getCurrent();

   ap.baselineErrorLimits.meanUpperBound =  baselineErrorMeanUpperBound.getCurrent();
   ap.baselineErrorLimits.meanLowerBound = baselineErrorMeanLowerBound.getCurrent();
   ap.baselineErrorLimits.stdDevPercent = baselineErrorStdDevPercent.getCurrent();
   ap.baselineErrorLimits.maxRange = baselineErrorMaxRange.getCurrent();


}

// -- end DxParametersInternal
// -- begin DxParameters




// Must pass command by value to avoid static init segfault

DxParameters::DxParameters(string command) : 
   SeekerParameterGroup(command, DbTableName, IdColNameInActsTable),
   internal_(new DxParametersInternal())
{
   internal_->daddResolution.addChoice(1);  // 1Hz
   internal_->daddResolution.addChoice(2);  // 2HZ
   internal_->daddResolution.addChoice(4);  // 4HZ

   internal_->sendBaselines.addChoice(ChoiceOn);
   internal_->sendBaselines.addChoice(ChoiceOff);

   internal_->sendComplexAmplitudes.addChoice(ChoiceOn);
   internal_->sendComplexAmplitudes.addChoice(ChoiceOff);

   internal_->dataRequestType.addChoice(DataReqFreq);
   internal_->dataRequestType.addChoice(DataReqSubchannel);

   internal_->manualBandwidth.addChoice(ChoiceOn);
   internal_->manualBandwidth.addChoice(ChoiceOff);

   internal_->allowPulseDetection.addChoice(ChoiceOn);
   internal_->allowPulseDetection.addChoice(ChoiceOff);

   internal_->sendBaselineStatistics.addChoice(ChoiceOn);
   internal_->sendBaselineStatistics.addChoice(ChoiceOff);

   internal_->checkBaselineWarningLimits.addChoice(ChoiceOn);
   internal_->checkBaselineWarningLimits.addChoice(ChoiceOff);

   internal_->checkBaselineErrorLimits.addChoice(ChoiceOn);
   internal_->checkBaselineErrorLimits.addChoice(ChoiceOff);

   internal_->recentRfiEnable.addChoice(ChoiceOn);
   internal_->recentRfiEnable.addChoice(ChoiceOff);

   addParameters();
   addAllImmedCmdHelp();

}

DxParameters::DxParameters(const DxParameters& rhs):
   SeekerParameterGroup(rhs.getCommand(), rhs.getDbTableName(), 
			rhs.getIdColNameInActsTable()),
   internal_(new DxParametersInternal(*rhs.internal_))
{
   setSite(rhs.getSite());
   addParameters();
}


DxParameters& DxParameters::operator=(const DxParameters& rhs)
{
   if (this == &rhs) {
      return *this;
   }
  
   setCommand(rhs.getCommand());
   eraseParamList();
   setSite(rhs.getSite());
   delete internal_;
   internal_ = new DxParametersInternal(*rhs.internal_);
   addParameters();
   return *this;
}

DxParameters::~DxParameters()
{
   delete internal_;
}

// sets all necessary parameters for class
void DxParameters::addParameters()
{
   addParam(internal_->dataCollectionLengthSecs);
   addParam(internal_->maxNumberOfCandidates);
   addParam(internal_->clusteringFreqTolerance);
   addParam(internal_->zeroDriftTolerance);
   addParam(internal_->maxDriftRateTolerance);
   addParam(internal_->cwClusteringDeltaFreq);
   addParam(internal_->badBandCwPathLimit);

   addParam(internal_->daddResolution);

   addParam(internal_->daddThreshold);

   addParam(internal_->cwCoherentThreshold);
   addParam(internal_->secondaryCwCoherentThreshold);
   addParam(internal_->secondaryPfaMargin);

   addParam(internal_->limitsForCoherentDetection);

   addParam(internal_->allowPulseDetection);
   addParam(internal_->pulseClusteringDeltaFreq);
   addParam(internal_->badBandPulseTripletLimit);
   addParam(internal_->badBandPulseLimit);

   addParam(internal_->pulseTrainSignifThresh);
   addParam(internal_->secondaryPulseTrainSignifThresh);

   addParam(internal_->maxPulsesPerHalfFrame);
   addParam(internal_->maxPulsesPerSubchannelPerHalfFrame);

   addParam(internal_->pulseThreshold);
   addParam(internal_->tripletThreshold);
   addParam(internal_->singletThreshold);
   addParam(internal_->baselineSubchannelAverage);
   addParam(internal_->baselineInitAccumHalfFrames);

   addParam(internal_->sendBaselines);
   addParam(internal_->sendBaselineStatistics);
   addParam(internal_->checkBaselineWarningLimits);
   addParam(internal_->checkBaselineErrorLimits);
   addParam(internal_->baselineReportingHalfFrames);
   addParam(internal_->sendComplexAmplitudes);
   addParam(internal_->dataRequestType);
   addParam(internal_->dataRequestSubchannel);
   addParam(internal_->dataRequestFreq);
   addParam(internal_->dataRequestMaxCompampSubchannels);


   addParam(internal_->manualBandwidth);
   addParam(internal_->bandwidth);

   addParam(internal_->baselineDecay);

   addParam(internal_->baselineWarningMeanUpperBound);
   addParam(internal_->baselineWarningMeanLowerBound);
   addParam(internal_->baselineWarningStdDevPercent);
   addParam(internal_->baselineWarningMaxRange);
   addParam(internal_->baselineErrorMeanUpperBound);
   addParam(internal_->baselineErrorMeanLowerBound);
   addParam(internal_->baselineErrorStdDevPercent);
   addParam(internal_->baselineErrorMaxRange);

   addParam(internal_->recentRfiEnable);
   addParam(internal_->recentRfiMaskElementWidthMinHz);
   addParam(internal_->recentRfiMaskSizeMax);

   sort();
}

const DxActivityParameters & DxParameters::getDxActParamStruct() const
{
   return internal_->getUpdatedDxActParam();
}

void DxParameters::dumpstruct() const
{
   cerr << getDxActParamStruct();
}

bool DxParameters::useManuallyAssignedDxBandwidth() const
{
   if (internal_->manualBandwidth.getCurrent() == ChoiceOn)
   {
      return true;
   }
   return false;
}

bool DxParameters::allowPulseDetection() const
{
   if (internal_->allowPulseDetection.getCurrent() == ChoiceOn)
   {
      return true;
   }
   return false;
}


int DxParameters::getDataCollectionLengthSecs() const
{
   return internal_->dataCollectionLengthSecs.getCurrent();
}

double DxParameters::getRecentRfiMaskElementWidthMinHz() const
{
   return internal_->recentRfiMaskElementWidthMinHz.getCurrent();
}

int DxParameters::getRecentRfiMaskSizeMax() const
{
   return internal_->recentRfiMaskSizeMax.getCurrent();
}
int DxParameters::getDataRequestMaxCompampSubchannels() const
{
   return internal_->dataRequestMaxCompampSubchannels.getCurrent();
}


// ---- DxOperation classes & methods

// A base class that runs through all the dxs in a list,
// executing the operation (which is deferred to the subclass).

class DxOperation
{
public:
   string runThroughDxList(const string &dxName,
			    Site *site);
   virtual ~DxOperation();
protected:
   // subclasses override this method
   virtual string callOperation(DxProxy *proxy) = 0; 

};

DxOperation::~DxOperation()
{
}

static bool compareDxsByName(DxProxy * dxFirst, DxProxy *dxSecond)
{
   return dxFirst->getName() < dxSecond->getName();
}


string DxOperation::runThroughDxList(const string &dxName,
				       Site *site)
{
   Assert(site);
   DxList dxList;
   site->dxManager()->getProxyList(&dxList);
   dxList.sort(compareDxsByName);

   string resultString;

   bool found = false;
   for (DxList::iterator index = dxList.begin();
	index != dxList.end(); ++index)
   {
      DxProxy *dxProxy = *index;
      Assert(dxProxy);
      if ( (dxName == "all") || dxProxy->getName() == dxName)
      {
	 found = true;
	 resultString += callOperation(dxProxy);
      }
   }

   if (found)
   {
      // if the command had no output text of its own,
      // return a generic acknowledgement.
      if (resultString.empty())
      {
	 resultString =  "dx command sent.";
      }
   }
   else 
   {
      resultString = "The requested DX(s) are not connected: " + dxName;
   }

   return resultString;

}

// request status
class DxReqStat : public DxOperation
{
protected:
   virtual string callOperation(DxProxy *proxy)
   {
      proxy->requestStatusUpdate();
      return "";
   }
};


class DxShutdown : public DxOperation
{
protected:
   virtual string callOperation(DxProxy *proxy) 
   {
      if (componentControlImmedCmdsGlobal.isComponentUnderControl(proxy->getName()))
      {
	 // Let the component controller perform a shutdown so that
	 // the component will not be restarted.
	 componentControlImmedCmdsGlobal.shutdown(proxy->getName());
      }
      else
      {
	 // Let the proxy handle shutdown on its own.
	 proxy->shutdown();
      }
      
      return "";
   }
};


class DxStop : public DxOperation
{
protected:
   virtual string callOperation(DxProxy *proxy) 
   {
      // stop any and all running activities
      proxy->stopDxActivity(NSS_NO_ACTIVITY_ID);
      return "";
   }
};




class DxRestart : public DxOperation
{
protected:
   virtual string callOperation(DxProxy *proxy)
   {
      if (componentControlImmedCmdsGlobal.isComponentUnderControl(proxy->getName()))
      {
	 componentControlImmedCmdsGlobal.restart(proxy->getName());
      }
      else
      {
	 // This has no effect on real dxs?
	 proxy->restart();
      }

      return "";
   }
};

class DxSendDataReq : public DxOperation
{
public:

   DxSendDataReq(const DxActivityParameters &dxActParamStruct)
      : dxActParamStruct_(dxActParamStruct)
   {}

protected:
   virtual string callOperation(DxProxy *proxy)
   {
      const DxScienceDataRequest &dataRequest =
	 dxActParamStruct_.scienceDataRequest;

      proxy->dxScienceDataRequest(dataRequest);
      return "";
   }

private:
   const DxActivityParameters &dxActParamStruct_;
};


// shorthand method for sending a complex amplitudes data request
//  by frequency

class DxSendDataReqFreq : public DxOperation
{
public:

   DxSendDataReqFreq(double rfFreqMhz, 
		      const DxActivityParameters &dxActParamStruct)
      :  rfFreqMhz_(rfFreqMhz),
	 dxActParamStruct_(dxActParamStruct)
   {}

protected:
   virtual string callOperation(DxProxy *proxy)
   {
      // grab a copy of the data request struct from the parameters
      // so that all the fields get filled in 
      DxScienceDataRequest dataRequest =
	 dxActParamStruct_.scienceDataRequest;

      dataRequest.sendComplexAmplitudes = SSE_TRUE;

      // now override to force a frequency request
      dataRequest.scienceData.requestType = REQ_FREQ;
      dataRequest.scienceData.rfFreq = rfFreqMhz_;

      proxy->dxScienceDataRequest(dataRequest);
      return "";
   }

private:
   double rfFreqMhz_;
   const DxActivityParameters &dxActParamStruct_;

};

// shorthand method for sending a complex amplitudes data request
//  by subchannel

class DxSendDataReqSubchannel : public DxOperation
{
public:

   DxSendDataReqSubchannel(int subchannel,
			 const DxActivityParameters &dxActParamStruct)
      :  subchannel_(subchannel),
	 dxActParamStruct_(dxActParamStruct)
   {}

protected:
   virtual string callOperation(DxProxy *proxy)
   {
      // grab a copy of the data request struct from the parameter
      // so that all the fields get filled in 
      DxScienceDataRequest dataRequest =
	 dxActParamStruct_.scienceDataRequest;

      dataRequest.sendComplexAmplitudes = SSE_TRUE;

      // now override to force a subchannel request
      dataRequest.scienceData.requestType = REQ_SUBCHANNEL;
      dataRequest.scienceData.subchannel = subchannel_;

      proxy->dxScienceDataRequest(dataRequest);
      return "";
   }

private:
   int subchannel_;
   const DxActivityParameters &dxActParamStruct_;

};




class DxResetSocket : public DxOperation
{
protected:
   virtual string callOperation(DxProxy *proxy)
   {
      proxy->resetSocket();
      return "";
   }
};

class DxStatusOperation : public DxOperation
{
protected:

   virtual string callOperation(DxProxy *proxy)
   {
      stringstream strm;
      const int HzPrecision(6);
      strm.precision(HzPrecision); // show N places after the decimal
      strm.setf(std::ios::fixed);  // show all decimal places up to precision

      strm << proxy->getName() << ": "
	   << proxy->getCachedDxStatus()
	   << " DxSkyFreq: " << proxy->getDxSkyFreq() << " MHz"
	   << "  Chan: " << proxy->getChannelNumber() 
	   << endl;
      return strm.str();
   }
};

class DxShownameOperation : public DxOperation
{
protected:

   virtual string callOperation(DxProxy *proxy)
   {
      stringstream strm;

      strm << proxy->getName() << " ";
	
      return strm.str();
   }
};




class DxConfigOperation : public DxOperation
{
protected:

   virtual string callOperation(DxProxy *proxy)
   {
      stringstream strm;

      strm << "\nDx Name: " << proxy->getName() << endl;
      strm << proxy->getConfiguration();
	
      return strm.str();
   }
};

class DxIntrinsicsOperation : public DxOperation
{
protected:

   virtual string callOperation(DxProxy *proxy)
   {
      stringstream strm;

      strm << "Dx Name: " << proxy->getName() << endl;
      strm << proxy->getIntrinsics();
	
      return strm.str();
   }
};

class DxSkyFreqOperation : public DxOperation
{
public:

   DxSkyFreqOperation(float64_t skyFreq)
      : skyFreq_(skyFreq)
   {}

protected:
   virtual string callOperation(DxProxy *proxy)
   {
      stringstream strm;

      strm << "setting skyfreq on dx: " << proxy->getName() 
	   << " to " << skyFreq_ << " MHz" << endl;
      proxy->setDxSkyFreq(skyFreq_);
      return strm.str();
   }

private:
   float64_t skyFreq_;
};


class DxChanOperation : public DxOperation
{
public:

   DxChanOperation(int32_t chanNumber)
      : chanNumber_(chanNumber)
   {}

protected:
   virtual string callOperation(DxProxy *proxy)
   {
      stringstream strm;

      strm << "setting channel on dx: " << proxy->getName() 
	   << " to " << chanNumber_ << endl;
      proxy->setChannelNumber(chanNumber_);
      return strm.str();
   }

private:
   int32_t chanNumber_;
};




// ---- immediate commands ---------

const char * DxParameters::reqstat(const char *dxName)
{
   DxReqStat reqStat;

   internal_->outString_ = reqStat.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
}

// Go through the variable length list of dx names
// and send a shutdown to each one.  The end of the list
// is assumed to be the first name that is an empty string.

const char * DxParameters::shutdown(
   const char *dxName0, const char *dxName1,
   const char *dxName2, const char *dxName3,
   const char *dxName4, const char *dxName5,
   const char *dxName6, const char *dxName7,
   const char *dxName8, const char *dxName9)
{
   list<string> requestedDxNames;

   requestedDxNames.push_back(dxName0);
   requestedDxNames.push_back(dxName1);
   requestedDxNames.push_back(dxName2);
   requestedDxNames.push_back(dxName3);
   requestedDxNames.push_back(dxName4);
   requestedDxNames.push_back(dxName5);
   requestedDxNames.push_back(dxName6);
   requestedDxNames.push_back(dxName7);
   requestedDxNames.push_back(dxName8);
   requestedDxNames.push_back(dxName9);


   string allnames;
   for (list<string>::iterator it = requestedDxNames.begin();
	it != requestedDxNames.end(); ++it)
   {
      const string & dxName = *it;
      allnames += dxName + " ";
   }
   SseArchive::SystemLog() << "UI request for dx shutdown:"
			   << allnames << endl;

   DxShutdown dxShutdown;
   internal_->outString_ = "";
   for (list<string>::iterator it = requestedDxNames.begin();
	it != requestedDxNames.end(); ++it)
   {
      const string & dxName = *it;

      if (dxName != "")
      {
	 internal_->outString_ += dxShutdown.runThroughDxList(dxName, getSite()) + "\n";
      } 
      else
      {
	 // found the first name that was empty, so we're done
	 break;
      }
   }
  
   return internal_->outString_.c_str();
}


const char * DxParameters::stop(const char *dxName)
{

   SseArchive::SystemLog() << "UI request for dx stop: " <<
      dxName <<endl;

   DxStop dxStop;

   internal_->outString_ = dxStop.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
}

const char * DxParameters::restart(const char *dxName)
{
   SseArchive::SystemLog() << "UI request for dx restart: " <<
      dxName <<endl;

   DxRestart dxRestart;

   internal_->outString_ = dxRestart.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
}

// send science data request (from current parameter settings)
const char * DxParameters::senddatareq(const char *dxName)
{
   DxSendDataReq dxSendDataReq(getDxActParamStruct());

   internal_->outString_ = dxSendDataReq.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();

}


// request compamp (science data) by rf freq
const char * DxParameters::reqfreq(double rfFreq, const char *dxName)
{

   // make sure freq value is in valid range
   double minReqFreqMhz = 0.0;
   double maxReqFreqMhz = AtaInformation::AtaMaxSkyFreqMhz;

   if (rfFreq < minReqFreqMhz || rfFreq > maxReqFreqMhz)
   {
      stringstream strm;
      strm << "reqfreq value " << rfFreq << " is out of range. \n"
	   << "Valid range: " << minReqFreqMhz << " to " 
	   << maxReqFreqMhz << " MHz" << endl;

      internal_->outString_ = strm.str();
      return internal_->outString_.c_str();
   }

   DxSendDataReqFreq dxSendDataReqFreq(rfFreq, getDxActParamStruct());
    
   internal_->outString_ = dxSendDataReqFreq.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
    
}

// request compamp (science data) by subchannel

const char * DxParameters::reqsub(int subchannel, const char *dxName)
{
   // make sure subchannel value is in valid range
   int minReqSubchannel = 0;
   int maxReqSubchannel = internal_->dataRequestSubchannel.getMax();   // TBD matches max dx subchannel

   if (subchannel < minReqSubchannel || subchannel > maxReqSubchannel)
   {
      stringstream strm;
      strm << "reqchan subchan value " << subchannel << " is out of range. \n"
	   << "Valid subchan range: " << minReqSubchannel << " to " 
	   << maxReqSubchannel << endl;

      internal_->outString_ = strm.str();
      return internal_->outString_.c_str();
   }

   DxSendDataReqSubchannel dxSendDataReqSubchannel(subchannel, getDxActParamStruct());
    
   internal_->outString_ = dxSendDataReqSubchannel.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
    
}


// reset socket (disconnect dx proxy)
const char * DxParameters::resetsocket(const char *dxName)
{

   SseArchive::SystemLog() << "UI request for dx socket reset: " <<
      dxName <<endl;

   DxResetSocket dxResetSocket;

   internal_->outString_ = dxResetSocket.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
}


const char *DxParameters::status(const char *dxName) const
{
   DxStatusOperation dxStatusOperation;

   internal_->outString_ = dxStatusOperation.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
}

const char *DxParameters::names() const
{
   DxShownameOperation dxShownameOperation;

   string dxName = "all";
   internal_->outString_ = dxShownameOperation.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
}


const char *DxParameters::config(const char *dxName)
{
   DxConfigOperation dxConfigOp;

   internal_->outString_ = dxConfigOp.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();
}

const char *DxParameters::intrin(const char *dxName) const
{
   DxIntrinsicsOperation dxIntrinsicsOp;

   internal_->outString_ = dxIntrinsicsOp.runThroughDxList(dxName, getSite());
   return internal_->outString_.c_str();

}

// load the specified parameter value into the dxProxy
const char *DxParameters::load(const char *paramName, const char *paramValue, 
				const char* dxName)
{
   string skyFreqParamName("skyfreq");
   string chanParamName("chan");

   string errorText;
   if (paramName == skyFreqParamName)
   {
      RangeParameter<float64_t> dxSkyFreq(
	 skyFreqParamName, "MHz", "dx center skyfreq",
	 DefaultDxSkyFreqMhz, -1, AtaInformation::AtaMaxSkyFreqMhz);
	
      // convert paramValue from string
      if (dxSkyFreq.convertFromString(paramValue, Parameter::CURRENT,
				       errorText))
      {
	 DxSkyFreqOperation dxSkyFreqOp(dxSkyFreq.getCurrent());
	 internal_->outString_ = dxSkyFreqOp.runThroughDxList(dxName, getSite());
      }
      else
      {
	 internal_->outString_ = errorText;
      }
   }
   else if (paramName == chanParamName)
   {
      const int32_t maxChan(255);
      RangeParameter<int32_t> chanNumber(
	 chanParamName, "number", "channel",
	 0, 0, maxChan);
	
      // convert paramValue from string
      if (chanNumber.convertFromString(paramValue, Parameter::CURRENT,
				       errorText))
      {
	 DxChanOperation dxChanOp(chanNumber.getCurrent());
	 internal_->outString_ = dxChanOp.runThroughDxList(dxName, getSite());
      }
      else
      {
	 internal_->outString_ = errorText;
      }
   }
   else
   {
      stringstream strm;

      strm << "Invalid parameter name: " << paramName << endl
	   << "Must be one of: " << skyFreqParamName << " " << chanParamName << endl;
      
      internal_->outString_ = strm.str();
   }

   return internal_->outString_.c_str();
}


bool DxParameters::recentRfiEnable() const
{
   if (internal_->recentRfiEnable.getCurrent() == ChoiceOn)
   {
      return true;
   }

   return false;
}


void DxParameters::addAllImmedCmdHelp()
{
   addImmedCmdHelp("config [<dxname>='all'] - display dx configuration");
   addImmedCmdHelp("dumpstruct - display dx activity parameters structure");
   addImmedCmdHelp("intrin [<dxname>='all'] - display dx intrinsics ");
   addImmedCmdHelp("load skyfreq <sky freq in MHz> <dxname | 'all'> - assign skyfreq to dx(s)");
   addImmedCmdHelp("load chan <channel number> <dxname | 'all'> - assign channel to dx(s)");
   addImmedCmdHelp("names - list all connected dxs");
   addImmedCmdHelp("reqfreq <sky freq in MHz> <dxname | 'all'> - request compamp data by sky freq");
   addImmedCmdHelp("reqstat [<dxname>='all'] - request dx status update ");
   addImmedCmdHelp("reqchan <subchan> <dxname | 'all'> - request compamp data by subchannel");
   addImmedCmdHelp("resetsocket <dxname | 'all'> - reset socket on dx(s)");
   addImmedCmdHelp("restart <dxname | 'all'> - restart dx(s)");
   addImmedCmdHelp("senddatareq <dxname | 'all'> - send science data request to dx(s)");
   addImmedCmdHelp("shutdown <dxname [dxname...] | 'all'> - shutdown dx(s) ");
   addImmedCmdHelp("status [<dxname>='all'] - display dx status  ");
   addImmedCmdHelp("stop <dxname | 'all'> - stop dx(s) ");

}
