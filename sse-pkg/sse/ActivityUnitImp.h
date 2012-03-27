/*******************************************************************************

 File:    ActivityUnitImp.h
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


// Activity Unit (partial) Implementation.
// Subclasses should inherit from this class,
// but clients should use the ActivityUnit abstract
// base class (protocol class).

#ifndef _activityunitimp_h
#define _activityunitimp_h

#include "ace/Synch.h"
#include "ActivityUnit.h"
#include "sseDxInterface.h"
#include "ObsSummaryStats.h"
#include "FollowUpSignalInfo.h"
#include "SetiLiveSignalInfo.h"
#include "ObserveActivityOps.h"
#include "SseException.h"
#include "TargetId.h"
#include "MutexBool.h"
#include <mysql.h>
#include <string>
#include <fstream>
#include <vector>

using std::vector;

class ObserveActivity;
class DxProxy;
class DxActivityParameters;
class ScienceDataArchive;
class CondensedSignalReport;
class ExpandedSignalReport;
class DbParameters;

typedef unsigned int DbTableKeyId;

class ActivityUnitImp : public ActivityUnit
{
 public:
  ActivityUnitImp (ObserveActivity *obsActivity,
		   int actUnitId,
		   DxProxy* dxProxy,
		   const DxActivityParameters &dxActParam,
		   int verboseLevel);
  virtual ~ActivityUnitImp();

  //---  methods called by activity ------
  virtual void initialize();
  virtual void dxScienceDataRequest(const DxScienceDataRequest &dataRequest);
  virtual void setStartTime(const StartActivity &startAct);
  virtual void stop(DbParameters &callerDbParam);
  virtual void shutdown();
  virtual void resetSocket();

  virtual string getDxName();
  virtual int getDxNumber();

  // methods called by activity, usually in a separate thread,
  // so must be thread safe:
  virtual void sendRecentRfiMask(MYSQL *callerDbConn, const vector<TargetId> & targetsToExclude, struct tm *refHHMMSS);
  virtual void prepareFollowUpCandidateSignals(MYSQL *callerDbConn, int previousActId);
  virtual int sendFollowUpCandidateSignals(const NssDate &newStartTime);
  virtual void sendCandidatesForSecondaryProcessing(MYSQL *callerDbConn);
  virtual void resolveCandidatesBasedOnSecondaryProcessingResults(MYSQL *callerDbConn);
  virtual void zxLookUpSetiLiveCandidates(MYSQL *callerDbConn);

  // -- methods called by dxProxy --------
  virtual void dxTuned(DxProxy* dx, const DxTuned &dxTuned);
  virtual void dataCollectionStarted (DxProxy* dx);
  virtual void dataCollectionComplete (DxProxy* dx);
  virtual void signalDetectionStarted (DxProxy* dx);
  virtual void signalDetectionComplete (DxProxy* dx);

  virtual void sendBaseline(DxProxy* dx,
			    const BaselineHeader &hdr,
			    BaselineValue valueArray[]);

  virtual void sendComplexAmplitudes(DxProxy* dx,
				     const ComplexAmplitudeHeader &hdr,
				     SubchannelCoef1KHz subchannelArray[]);

  virtual void sendBaselineStatistics(DxProxy *dx,
				      const BaselineStatistics &baselineStats);

  virtual void baselineWarningLimitsExceeded(DxProxy *dx,
					     const BaselineLimitsExceededDetails &details);

  virtual void baselineErrorLimitsExceeded(DxProxy *dx,
					     const BaselineLimitsExceededDetails &details);

  virtual void beginSendingCandidates(DxProxy* dx, const Count &count);
  virtual void sendCandidatePulseSignal(DxProxy* dx,
					const PulseSignalHeader &hdr,
					Pulse pulses[]);
  virtual void sendCandidateCwPowerSignal(DxProxy* dx,
					  const CwPowerSignal &cwPowerSignal);
  virtual void doneSendingCandidates(DxProxy* dx);


  virtual void beginSendingSignals(DxProxy* dx, const DetectionStatistics &stats);
  virtual void sendPulseSignal(DxProxy* dx,
			       const PulseSignalHeader &hdr,
			       Pulse pulses[]);
  virtual void sendCwPowerSignal(DxProxy* dx,
				 const CwPowerSignal &cwPowerSignal);
  virtual void doneSendingSignals(DxProxy* dx);

  virtual void beginSendingCwCoherentSignals(DxProxy* dx, const Count &count);
  virtual void sendCwCoherentSignal(DxProxy* dx,
				 const CwCoherentSignal &cwCoherentSignal);
  virtual void doneSendingCwCoherentSignals(DxProxy* dx);

  virtual void beginSendingBadBands(DxProxy *dx, const Count &count);
  virtual void sendCwBadBand(DxProxy *dx, const CwBadBand &cwBadBand);
  virtual void sendPulseBadBand(DxProxy *dx, const PulseBadBand &pulseBadBand);
  virtual void doneSendingBadBands(DxProxy *dx);


  // secondary processing results:
  virtual void beginSendingCandidateResults(DxProxy* dx, const Count &count);
  virtual void sendPulseCandidateResult(DxProxy* dx,
					const PulseSignalHeader &hdr,
					Pulse pulses[]);
  virtual void sendCwCoherentCandidateResult(DxProxy* dx, const CwCoherentSignal &cwCoherentSignal);
  virtual void doneSendingCandidateResults(DxProxy* dx);

  virtual void archiveComplete(DxProxy *dx);

  virtual void notifyDxProxyDisconnected(DxProxy *dxProxy);

  virtual void sendDxMessage(DxProxy *dx, const NssMessage & nssMessage);

  // Warning ActivityUnit may be destroyed in this method.
  // Do not use ActivityUnit past this point.
  virtual void dxActivityComplete(DxProxy *dx, const DxActivityStatus &status);

  
 protected:

  virtual int getId();
  virtual int getActivityId();

  virtual void createSignalReportFileStream(ofstream &strm,
					    const string &outputFilePrefix,
					    const string & dxId);
  virtual void createActUnitSummaryFileStream();
  virtual void writeActUnitSummaryHeader();
  virtual void activityUnitComplete();
  virtual int getVerboseLevel();
  virtual ObserveActivity *getObsAct();
  virtual ScienceDataArchive *getScienceDataArchive();
  virtual ObsSummaryStats & getObsSummaryStats();
  virtual ostream & getSigReportTxtStrm();
  virtual CondensedSignalReport * getCondensedAllSignalReport();
  virtual CondensedSignalReport * getCondensedCandidateReport();
  virtual ExpandedSignalReport * getExpandedAllSignalReport();
  virtual ExpandedSignalReport * getExpandedCandidateReport();
  virtual string getOutputFilePrefix();
  virtual string getDxTuningInfo(const DxActivityParameters & dxActParam,
                                  double hzPerDxSubchannel,
                                  int numberOfSubchannels);

  virtual void terminateActivityUnit(DbParameters &callerDbParam,
                                     SseException &except);
  virtual void forwardFollowUpSignalsToDx(FollowUpSignalInfoList &infoList,
					   const NssDate &newStartTime);
  virtual void projectSignalAheadInFreq(FollowUpSignal &sigInfo,
					const NssDate &newStartTime);

  virtual FollowUpSignalInfo & lookUpFollowUpSignalById(int signalIdNumber);

  void sendArchiveRequestsForSecondaryCandidates();
  void sendArchiveRequestsForPrimaryCandidates();
  void determineArchiveRequest(DxProxy *dx,
			       const SignalDescription &descrip);

  void sendScienceDataRequestFreq(double rfFreqMhz);

  bool actOpsBitEnabled(const ObserveActivityOperations & opsBit);

  virtual void getRecentRfiSignals(
     MYSQL *callerDbConn, double beginFreqMhz, double endFreqMhz,
     const vector<TargetId> & targetIdsToExclude,
     vector<double> & signalFreqMhz);

  virtual void sendRecentRfiMaskToDx(
     double bandCoveredCenterFreqMhz, 
     double bandCoveredWidthMhz,
     const vector<double> & maskCenterFreqMhz, 
     const vector<double> & maskWidthMhz, 
     unsigned int maskSizeToUse);

  virtual int adjustRecentRfiMaskSize(int currentMaskSize);

  virtual void recordObsHistoryInDatabase();
  virtual void recordDxSettingsInDatabase();

  unsigned int getDbActivityUnitId();
  void updateDbErrorComment(MYSQL *callerDbConn, const string& comment);
  void updateStats();
  void updateObsSummarySignalCounts(SignalClassReason reason);
  void logCompactBaselineStats(const BaselineStatistics &stats);

  // -- zx processing
  void getSetiLiveCandidates(MYSQL *callerDbConn);
  // -- pulse signals ---
  virtual DbTableKeyId recordCandidate(const PulseSignalHeader &pulseSignalHdr,
                              Pulse pulses[], const string& location);

  virtual void recordSignal(const PulseSignalHeader &pulseSignalHdr,
                              Pulse pulses[], const string& location);

  virtual DbTableKeyId record(const string &signalTableName,
			      const string &pulseTrainTableName,
                              const PulseSignalHeader &pulseSignalHdr,
                              Pulse pulses[], const string& location);


  virtual void savePulseCandSigInfo(DbTableKeyId dbTableKeyId, 
			    const SignalDescription & descrip,
			    const ConfirmationStats &cfm);
  
  virtual void saveCwCohCandSigInfo(DbTableKeyId dbTableKeyId, 
			    const SignalDescription & descrip,
			    const ConfirmationStats & cfm);

  virtual void saveSecondarySignalDescrip(const SignalDescription &descrip);

  // -- cw power signals

  virtual void recordSignal(const CwPowerSignal& cwPowerSignal, 
			      const string& location);

  virtual void recordCandidate(const CwPowerSignal& cwPowerSignal, 
			      const string& location);

  virtual void record(const string & signalTableName,
		      const CwPowerSignal & cwPowerSignal, 
		      const string & location);


  // -- cw coherent signals

  virtual DbTableKeyId recordCandidate(const CwCoherentSignal& cwCoherentSignal, 
			      const string& location);

  virtual const string prepareDatabaseRecordStatement();

  virtual void putCommonSignalInfoIntoSqlStatement(
      ostream & sqlstmt, const SignalDescription & sig,
      const string &sigTypeString, const string &location);

  virtual void putSignalDescriptionIntoSqlStatement(ostream &sqlstmt, 
						    const SignalDescription &sig);
  virtual void putSignalIdIntoSqlStatement(ostream &sqlstmt, const SignalId &signalId);
  virtual void putOrigSignalIdIntoSqlStatement(ostream &sqlstmt,
					       const SignalId &origSignalId);
  virtual void putConfirmationStatsIntoSqlStatement(ostream & sqlstmt, 
						    const ConfirmationStats & cfm);

  virtual void recordDetectionStats(DxProxy *proxy,
				    const DetectionStatistics &stats); 

  virtual void recordBadBandInDb(DxProxy *proxy, const CwBadBand &cwBadBand);
  virtual void recordBadBandInDb(DxProxy *proxy, const PulseBadBand &pulseBadBand);

  virtual void recordBaselineStatsInDb(
      DxProxy *proxy, const BaselineStatistics &baselineStats);

  virtual void submitDbQueryWithLoggingOnError(MYSQL *conn,
     const string &sqlStmt, const string & callingMethodName, int lineNumber);

  virtual void submitDbQueryWithThrowOnError(MYSQL *conn,
     const string &sqlStmt, const string & callingMethodName, int lineNumber);

  DxActivityParameters dxActParam_;

 private:

  friend class LookUpCandidatesFromPrevAct;
  friend class LookUpCandidatesFromSetiLive;
  friend class LookUpCandidatesFromCounterpartDxs;
  friend class ResolveCandidatesBasedOnSecondaryProcessingResults;
  friend class PrepareFakeSecondaryCandidatesToForceArchiving;

  struct PulseTrain
  {
      PulseSignalHeader hdr;
      Pulse *pulseArray;

      PulseTrain();
      virtual ~PulseTrain();
  };

  typedef std::list<CwPowerSignal> CwPowerSignalList;
  typedef std::list<CwCoherentSignal> CwCoherentSignalList;
  typedef std::list<PulseTrain *> PulseTrainList;
  typedef std::list<SignalDescription> SigDescripList;

  struct CandSigInfo
  {
      DbTableKeyId dbTableKeyId;
      string sigType;
      SignalDescription descrip;
      ConfirmationStats cfm;

      CandSigInfo();
  };
  typedef std::list<CandSigInfo> CandSigInfoList;
  typedef std::list<CandSigInfo> SetiLiveSignalInfoList;

  // Methods
  void saveSignalReports(); 
  void sendCandidateReportsToSystemLog();
  void saveRecentRfiMaskToArchiveFile(const RecentRfiMaskHeader &header, 
				      FrequencyBand freqBandArray[]);

  void getCandidatesFromCounterpartDxs(
      CwPowerSignalList &cwPowerSigList,
      PulseTrainList &pulseTrainList,
      CwCoherentSignalList &cwCoherentSigList);

  void saveCandSigInfo(DbTableKeyId dbTableKeyId, 
		       const string &sigType,
		       const SignalDescription & descrip,
		       const ConfirmationStats & cfm);

  void reclassifyCandidateSignal(SignalDescription &descrip, 
                                 ConfirmationStats &cfm);

  bool signalPassedOffActNullBeamSnrTest(SignalDescription &descrip,
                                         ConfirmationStats &cfm);

  void updateStartTimeAndDxSkyFreqInDb(const StartActivity& startAct);
  void detachSelfFromDxProxy();
  void releaseResources();
  void sendStopMsgToDx();
  int getSubchannel(double freq);
  void sendRequestedCompampSubchannels(const vector<int> &compampSubchannels);

  // Variables
  ObserveActivity* obsActivity_;
  DxProxy* dxProxy_;
  int verboseLevel_;
  ScienceDataArchive *scienceDataArchive_;
  CondensedSignalReport *condensedAllSignalReport_;
  CondensedSignalReport *condensedCandidateReport_;
  ExpandedSignalReport *expandedAllSignalReport_;
  ExpandedSignalReport *expandedCandidateReport_;
  int actUnitId_;
  unsigned int dbActivityUnitId_;
  double dxBandLowerFreqLimitMHz_;
  double dxBandUpperFreqLimitMHz_;
  double actualDxTunedFreqMhz_;

  ofstream sigReportTxtStrm_;   // signal report output stream, text format
  ObsSummaryStats obsSummaryStats_;
  ofstream actUnitSummaryTxtStrm_;   // actUnit summary output stream

  FollowUpSignalInfoList followUpSignalInfoList_;

  DbParameters & dbParam_;
  MYSQL *dbConn_;
  bool useDb_;
  bool zxMode_;

  int beamNumber_;
  TargetId targetId_;
  string siteName_;

  CandSigInfoList candSigInfoList_;
  SigDescripList secondarySignalDescripList_;
  bool detachedSelfFromDxProxy_;
  bool wrappedUp_;
  MutexBool stopCommandReceived_;

  bool usingOffActNull_;
  double nullDepthLinear_;

  ACE_Recursive_Thread_Mutex wrappedUpMutex_;
  ACE_Recursive_Thread_Mutex detachFromDxProxyMutex_;
  ACE_Recursive_Thread_Mutex actUnitSummaryTxtStrmMutex_;

  // Disable copy construction & assignment.
  // Don't define these.
  ActivityUnitImp(const ActivityUnitImp& rhs);
  ActivityUnitImp& operator=(const ActivityUnitImp& rhs);
  

};

#endif
