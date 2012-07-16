/*******************************************************************************

 File:    ObserveActivityImp.h
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

#ifndef _observeactivityimp_h
#define _observeactivityimp_h

// ObserveActivity implementation.
#include <ace/Task_T.h>
#include <ace/Synch.h>
#include <ace/Atomic_Op.h>
#include "ObserveActivity.h"
#include "DebugLog.h"
#include "Timeout.h"
#include "ActUnitList.h"
#include "sseDxInterface.h"
#include "ObserveActivityOps.h"
#include "DxOpsBitset.h"
#include "ObserveActivityStatus.h"
#include "ObsSummaryStats.h"
#include "ActParameters.h"
#include "IfcParameters.h"
#include "TscopeParameters.h"
#include "TestSigParameters.h"
#include "SchedulerParameters.h"
#include "DbParameters.h"
#include "DxParameters.h"
#include "TscopeList.h"
#include "TestSigList.h"
#include "IfcList.h"
#include "DxList.h"
#include "ChannelizerList.h"
#include "Assert.h" 
#include "MutexBool.h"
#include "sseTscopeInterface.h"
#include "TargetId.h"
#include "ActUnitListMutexWrapper.h"
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>

class ActivityUnit;
class ObsSummaryStats;
class NssComponentTree;
class NssParameters;
class ActivityStrategy;
struct TscopeTargetRequest;
class IfDbOffsetTable;
struct IfcStatus;
class ExpectedNssComponentsTree;

using std::vector;

class ObserveActivityImp : public ObserveActivity
{
public:

  ObserveActivityImp(ActivityId_t id,
		     ActivityStrategy* activityStrategy,
		     const string &activityName,
		     NssComponentTree *nssComponentTree,
		     const NssParameters& nssParameters,
		     const ObserveActivityOpsBitset &actOpsBitset,
		     const DxOpsBitset &dxOpsBitset,
		     int verboseLevel);

  virtual ~ObserveActivityImp();

  virtual bool start();
  virtual void stop();
  virtual void destroy();

  // incoming messages from activityUnits
  virtual void activityUnitReady(ActivityUnit *activityUnit);
  virtual void dataCollectionStarted(ActivityUnit *activityUnit);
  virtual void dataCollectionComplete(ActivityUnit *actUnit);
  virtual void signalDetectionStarted(ActivityUnit* activityUnit);
  virtual void signalDetectionComplete(ActivityUnit* activityUnit);
  virtual void doneSendingCwCoherentSignals(ActivityUnit* actUnit);
  virtual void doneSendingCandidateResults(ActivityUnit* activityUnit);
  virtual void activityUnitObsSummary(ActivityUnit *activityUnit,
				      ObsSummaryStats &obsSumStats);
  virtual void activityUnitFailed(ActivityUnit *activityUnit);
  virtual void activityUnitComplete(ActivityUnit *activityUnit);

  // incoming messages from other components
  virtual void tscopeReady(TscopeProxy* tscopeProxy);
  virtual void tscopeCancelTimer(TscopeProxy* tscopeProxy);
  virtual void testSigReady(TestSigProxy* testSigProxy);
  virtual void ifcReady(IfcProxy* ifcProxy);

  virtual void processNssMessage(NssMessage &nssMessage);
  virtual void ifcError(IfcProxy * ifcProxy, NssMessage &nssMessage);
  virtual void tscopeError(TscopeProxy * tscopeProxy, 
			   NssMessage &nssMessage);

  virtual void testSigError(TestSigProxy * testSigProxy, 
			    NssMessage &nssMessage);

  // utilities
  virtual const string getStatus() const;
  virtual const string getActivityName() const;
  virtual const string getActivityType() const;

  virtual string getArchiveFilenamePrefix() const;
  virtual string getDataProductsDir();

  virtual int getStartTime() const;
  virtual int getStartBaseAccumTime() const;
  virtual int getStartDataCollTime() const;
  virtual NssDate getStartTimeAsNssDate() const;
  virtual NssDate getStartBaseAccumTimeAsNssDate() const;
  virtual NssDate getStartDataCollTimeAsNssDate() const;
  virtual DbParameters & getDbParameters();
  void updateDbErrorComment(DbParameters &dbParam, const string& emsg);
  void updateIfcStatus(const IfcStatus& ifcStatus);
  void updateActivityStatistics();
  void updateActivityStartDataCollTime();

  virtual void lookUpTargetCoordinates(
     MYSQL *conn, TargetId targetId,
     double *ra2000Rads, double *dec2000Rads, bool *moving);

  virtual double calculateSynthBeamsizeRads(double skyFreqMhz);

  ActParameters & getActParameters();
  const DxParameters & getDxParameters();
  ObserveActivityOpsBitset & getOperations();

  int getPrevActStartTime();
  int getBeamNumberForDxName(const string & dxName);
  TargetId getTargetIdForBeam(int beamNumber);
   
  string getSiteName();

  string prepareObsSummStatsSqlUpdateStmt(
     const ObsSummaryStats & obsSummaryStats);

protected:

  typedef std::multimap<TscopeTuning, string> AtaTuningToPreludeBeamsMultiMap;

  struct TargetCoordinates
  {
     double ra2000Rads;
     double dec2000Rads;
     
     TargetCoordinates();
  };

  struct TargetInfo {
     string preludeBeamName;
     TargetId targetId;
     double tuningSkyFreqMhz;
     TargetCoordinates coords;

     TargetInfo();
  };


  /*
    Work task (thread) for doing time consuming
    procedures.
   */
  class WorkTask: public ACE_Task<ACE_MT_SYNCH>
  {
  public:
     WorkTask();
     virtual int start();
     virtual int svc(void);

  private:

  };

  virtual void startComponents(); 
  virtual void activityFailed();
  virtual void activityComplete();
  virtual void finishNonDxActivity();
  virtual void terminateActivity(const string &errorMsg);
  virtual void stopActivity();
  virtual void stopActivityUnits();
  virtual void detachAllNonDxComponents();
  virtual void deleteAllActivityUnits();

  virtual void determineDesiredComponents();
  virtual void checkModesThatRequireDatabaseToBeOn();
  virtual void prepareForForcedArchivingAroundCenterTuning();
  virtual void prepareActivityUnits(const ActUnitList &actUnitList);
  virtual int calculateStartBaseAccumTime();
  virtual int calculateStartDataCollTime();
  virtual int calculateStartTime();
  virtual void sendStartBaseAccumTime(const NssDate & startTime);
  virtual void sendStartDataCollTime(const NssDate & startTime);
  virtual void handleTscopeReadyTimeout();
  virtual void handleTestSigReadyTimeout();
  virtual void handleIfcReadyTimeout();
  virtual void processTunedDxs();
  virtual void beginDxPreparation();
  virtual void completeRemainingDxPreparation();

  virtual void handleDxTimeout(
      const string & expectedMessageDescrip,
      const ActUnitList & actUnitsReportedIn,
      void (ObserveActivityImp::*methodToCallOnSuccess)());
  virtual void handleDxTunedTimeout();
  virtual void handleDataCollectionCompleteTimeout();
  virtual void handleDoneSendingCwCoherentSignalsTimeout();
  virtual void handleZxLookUpSetiLiveCandidatesTimeout();
  virtual void handleActUnitCompleteTimeout();
  virtual void prepareFollowupCandidateSignals();
  virtual int sendFollowupCandidateSignals(const NssDate &startTime);
  virtual void startActUnitWatchdogTimers();
  virtual void sendRecentRfiMask();

  virtual void getTargetInfo(MYSQL *conn, TargetId targetId,
   double *ra2000Rads, double *dec2000Rads, bool *moving);

  virtual void calculateMovingTargetPosition(MYSQL *conn,
     TargetId targetId, double *ra2000Rads, double *dec2000Rads);

  string getSpacecraftEphemFilename(MYSQL *conn, TargetId targetId);

  int calculateDeltaTimeBetweenObsSecs(int prevActStartTime);

  virtual double calculatePrimaryBeamsizeRads(double skyFreqMhz);

  virtual void logBeamsize(const string &beamType, double beamsizeRads);

  virtual void getPreviousActivityInfoFromDb(ActivityId_t prevActivityId);
  virtual void logPosition(const string &description, 
			   double ra2000Rads, double dec2000Rads);

  virtual void logObsSummaryStats();
  virtual void verifyMinMaxDxSkyFreqs();
  virtual void mailStartingObservationMessage();
    
  virtual void updateStatus(ObserveActivityStatus::StatusEnum statusEnum);
  virtual void updateActivityLog();
  virtual void adjustDxActivityParameters(DxActivityParameters &ap,
					   const DxOpsBitset &dxOpsBitset);

  virtual void prepareDataProductsArchive();
  virtual void createObsSummaryFileStream();
  virtual void writeObsSummaryHeader(const NssParameters &nssParameters);
  virtual void processAllDataCollectionComplete();
  virtual void processAllDoneSendingCwCoherentSignals();
  virtual void doNothing();
  virtual void allDataCollectionCompleteWrapup();

  virtual DxList & getDxList();
  virtual double getMinDxSkyFreqMhz() const;
  virtual double getMaxDxSkyFreqMhz() const;
  virtual IfcList & getIfcList();
  virtual ostream & getObsSummaryTxtStrm();
  virtual ObserveActivityOpsBitset & getActOpsBitset();
  virtual NssComponentTree *getNssComponentTree();

  virtual bool useManuallyAssignedDxBandwidth();
  virtual void compareDxDataResults();
  virtual void logError(const string &msg);

  virtual void startWatchdogTimer(
     Timeout<ObserveActivityImp> & timeout,
     void (ObserveActivityImp::*methodToCallOnTimeout)(),
     int waitDurationSecs);

  virtual void cancelPendingTimers();

  virtual void setIfSource();
  virtual void startTelescopes();
  virtual void startTestSigGens();
  virtual void startIfcs();
  virtual void startDxs();

  virtual void turnOffTscopes();
  virtual void turnOffTestSigs();
  virtual void turnOffIfcs();

  bool validIfcStatus(IfcProxy* ifcProxy);

  virtual void determineDesiredOperations();
  
  virtual void setCwTestSignal(TestSigProxy* testSigProxy, 
			       const TestSigParameters & testSigParameters);
  virtual void setPulseTestSignal(TestSigProxy* testSigProxy, 
				  TestSigParameters & testSigParameters);
  virtual void checkAvailableArchiveDiskSpace(const string &disk);
  virtual IfcParameters & getIfcParametersForIfcName(const string &ifcName);

  virtual void verifyDxMinMaxFreqsFallWithinAtaTuningBand(
     const string &ataTuningName, double dxMinFreqMhz, double dxMaxFreqMhz,
     double tuningCenterMhz, double tuningBandwidthMhz);

  virtual void createActivityUnit(DxProxy *dxProxy, int actUnitId,
					ActUnitList &actUnitList);
  
  virtual void setIfcAttenuators(IfcParameters &ifcParams,
				 double ifcSkyFreqMhz,
				 IfcProxy *ifcProxy);

  virtual void sendCandidatesToDxsForSecondaryProcessing();
  virtual void resolveCandidatesBasedOnSecondaryProcessingResults();

  virtual string getTargetIdsForAllBeamsInUse();

  virtual void determineAtaTuningFreqsForPreludeBeamsInUse(
     const AtaTuningToPreludeBeamsMultiMap & ataTuningToPreludeBeamsMultiMap);

  virtual void commandAtaTuningsForPreludeBeamsInUse(
     const AtaTuningToPreludeBeamsMultiMap & ataTuningToPreludeBeamsMultiMap,
     TscopeProxy *tscopeProxy);

  virtual void pointBeams(TscopeProxy *tscopeProxy);

  virtual void getNominalTargetPositionsForPreludeBeams(
     const vector<string> & preludeBeamsToUse, 
     vector<TargetInfo> & nominalTargetPositions);

  virtual void findOffPositions(
     const vector<TargetInfo> & nominalTargetPositions,
     double primaryBeamsizeRads, 
     double synthBeamsizeRads, 
     double primaryCenterRaRads, double primaryCenterDecRads,
     vector<TargetInfo> & adjustedTargetPositions);

  virtual void findGridPositions(
     const vector<TargetInfo> & nominalTargetPositions,
     double synthBeamsizeRads, 
     vector<TargetInfo> & adjustedTargetPositions);

  vector<string> getAtaBeamsForPreludeBeam(const string & preludeBeamName);

  void assignSubarraysToAtaBeams(TscopeProxy *tscopeProxy,
                                 const vector<string> & preludeBeamsToUse);

  void assignTargetsToAtaBeams(TscopeProxy *tscopeProxy,
                               const vector<TargetInfo> & targets);

  void prepareNullBeams(TscopeProxy *tscopeProxy, 
                        const vector<TargetInfo> & nominalTargets,
                        const vector<TargetInfo> & adjustedTargets);

  void assignCrossBeamProjectionNulls(TscopeProxy *tscopeProxy,
                                      const vector<TargetInfo> & targets);

  void assignOffActProjectionNulls(TscopeProxy *tscopeProxy, 
                                   const vector<TargetInfo> & targets);
  
  void addNullBeamCoordsToAtaBeams(TscopeProxy * tscopeProxy,
                                   const string & preludeBeamName,
                                   TscopeBeamCoords &coords);

  void pointBeamformer(TscopeProxy *tscopeProxy);

  void preparePrimaryBeam(TscopeProxy *tscopeProxy,
                          double primaryCenterRaRads,
                          double primaryCenterDecRads);

  virtual void calculatePrimaryBeamPointing(double * ra2000Rads,
					    double * dec2000Rads);

  virtual double determineTuningSkyFreqMhzForPreludeBeam(
     const string & preludeBeamName);

  virtual void setAtaBeamCoords(TscopeProxy *tscopeProxy, 
                                const string &ataBeamName, int preludeBeamNumber,
                                double ra2000Rads, double dec2000Rads);
  
  virtual void pointAtaSubarray(TscopeProxy *tscopeProxy, 
                                const string &subarray,
                                double ra2000Rads, double dec2000Rads);

  virtual void pointAtaSubarrayAzEl(TscopeProxy *tscopeProxy, 
                                    const string &subarray,
                                    double azDeg, double elDeg);
  
  virtual void requestAtaSubarrayPointingCheck(TscopeProxy *tscopeProxy, 
                                const string &subarray,
                                double ra2000Rads, double dec2000Rads);
  
  virtual void assignAtaSubarrayToAtaBeam(TscopeProxy *tscopeProxy,
                                          const string &ataBeamName,
                                          const string &subarray);

  virtual void findAtaTuningsForPreludeBeamsInUse(
     AtaTuningToPreludeBeamsMultiMap & ataTuningToPreludeBeamsMultiMap);

  virtual void determineAtaTuning(
     TscopeTuning tuning,
     const AtaTuningToPreludeBeamsMultiMap & ataTuningToPreludeBeamsMultiMap);

  virtual void sendAutoselectAntsCommands(TscopeProxy *tscopeProxy);
  virtual void tscopeSetup(TscopeProxy*);
  virtual void sendPrepareAntsCommands(TscopeProxy *tscopeProxy);
  virtual void sendFreeAntsCommands(TscopeProxy *tscopeProxy);
  virtual void beamformerReset(TscopeProxy *tscopeProxy);
  virtual void beamformerInit(TscopeProxy *tscopeProxy);
  virtual void beamformerAutoatten(TscopeProxy *tscopeProxy);
  virtual void pointAntsAndWait(TscopeProxy *tscopeProxy);

  virtual void determineAntLists();
  virtual vector<string> prepareAntList(const string & listName, const string & antList);
  virtual void checkAntListSubset(const string &subsetListName, vector<string> & subsetList,
                                  const string & masterListName, vector<string> & masterList);

  virtual void findTargetsInPrimaryBeamFov(MYSQL *conn,
                                           double obsFreqMhz,
                                           vector<TargetId> & targetIdsInFov);

  virtual void findTargetsInRaDecRanges(
     MYSQL *conn,
     double minRaLimitHours, double maxRaLimitHours,
     double minDecLimitDeg, double maxDecLimitDeg,
     vector<TargetId> & targetIdsInFov);
  
  // convenience methods for checking operations bits
  virtual bool observingTargets();
  virtual bool useTscope();
  virtual bool useRf();
  virtual bool useIfc();
  virtual bool useDx();
  virtual bool useTestgen();
  virtual bool createRecentRfiMaskEnabled();
  virtual bool followUpObservationEnabled();
  virtual bool isCalObservation();
  virtual bool isAutoselectAntsActivity();
  //JR - Tscope Setup
  virtual bool isTscopeSetupActivity();
  virtual bool isPrepareAntsActivity();
  virtual bool isFreeAntsActivity();
  virtual bool isBeamformerResetActivity();
  virtual bool isBeamformerInitActivity();
  virtual bool isBeamformerAutoattenActivity();
  virtual bool isPointAntsAndWaitActivity();
  virtual bool isOffObservation();
  virtual bool isOnObservation();
  virtual bool isGridWestObservation();
  virtual bool isGridSouthObservation();
  virtual bool isGridOnObservation();
  virtual bool isGridNorthObservation();
  virtual bool isGridEastObservation();
  virtual bool isMultitargetObservation();
  virtual bool classifyAllSignalsAsRfiScan();
  virtual bool forceArchivingAroundCenterTuning();
  virtual bool doNotReportConfirmedCandidatesToScheduler();

  // methods for getting the appropriate component lists
  // subclass may override as needed

  virtual TscopeList & getAllNeededTscopes();
  virtual TestSigList & getAllNeededTestSigs(); 

  TestSigParameters & getTestSigParameters(const string &testSigName );

  int getPrevActStartTime() const; 
  void loadIfAttnDbOffsetTable();
  int calculateStartTimeOffset();

  void storeTuningNameForPreludeBeam(const string &preludeBeamName,
				     const string &tuningName);

  string getTuningNameForPreludeBeam(const string &preludeBeamName);

  void recordTscopeStatusInDatabase(
     const TscopeStatusMultibeam & multibeamStatus);

  void recordPointRequestInDatabase(
     int beamNumber, const string & beamName,
     const TscopePointing & pointing);

  void recordBeamStatusInDatabase(const string & beamName, 
				  const TscopePointing & pointing);
  
  void recordSubarrayStatusInDatabase(
     const string & beamName, const TscopeSubarrayStatus & status);

  double percentComplete() const;

  virtual void setDiskStatusMsg(const string &msg);
  virtual string getDiskStatusMsg() const;

  // ----- database -----
  void storeParametersInDb();
  void recordParameters();
  void checkForZeroActId();

  void submitDbQueryWithLoggingOnError(
     const string &sqlStmt,
     const string &callingMethodName,
     int lineNumber);
  
  void submitDbQueryWithThrowOnError(
     const string &sqlStmt,
     const string &callingMethodName,
     int lineNumber);

  void submitDbQueryWithThrowOnError(
     DbParameters &dbParam,
     const string &sqlStmt,
     const string &callingMethodName,
     int lineNumber);

private:

  void deleteSelf();

  const string activityName_;
  NssComponentTree *nssComponentTree_;
  ObserveActivityStatus statusStage_;

  ActUnitListMutexWrapper actUnitStillWorkingListMutexWrapper_;
  ActUnitList actUnitCreatedList_;
  ActUnitList actUnitReadyList_;
  ActUnitList actUnitDataCollCompleteList_;
  ActUnitList actUnitDoneSendingCwCoherentSignalsList_;
  ActUnitList actUnitCompleteList_;

  DxList dxList_;
  IfcList ifcList_;
  TscopeList tscopeList_;

  string xpolAnts_;
  string ypolAnts_;
  string primaryAnts_;

  ActParameters actParameters_;
  TscopeParameters tscopeParameters_;
  IfcParameters ifc1Parameters_;
  IfcParameters ifc2Parameters_;
  IfcParameters ifc3Parameters_;
  TestSigParameters testSig1Parameters_;
  TestSigParameters testSig2Parameters_;
  TestSigParameters testSig3Parameters_;
  SchedulerParameters schedulerParameters_;
  DbParameters dbParameters_;
  DbParameters dbParametersForWorkThread_;
  DbParameters dbParametersForTerminate_;
  DxParameters dxParameters_;
  DxActivityParameters dxActParameters_;
  string siteName_;
  ExpectedNssComponentsTree *expectedTree_;

  int nTscopesStarted_;
  int nTscopesReady_;
  int nTestSigsStarted_;
  int nTestSigsReady_;
  int nIfcsStarted_;
  int nIfcsReady_;

  int nActUnitsStarted_;  // how many activity units have been started
  int nActUnitsStillWorking_;  // how many act units are still functioning
  int nActUnitsReady_;    // how many have reported in as ready
  int nActUnitsDataCollectionStarted_;     
  int nActUnitsDataCollectionComplete_;    
  int nActUnitsSignalDetectionStarted_;     
  int nActUnitsSignalDetectionComplete_;    
  int nActUnitsDoneSendingCwCoherentSignals_;
  int nActUnitsDoneSendingCandidateResults_;
  int nActUnitsDone_;     // how many are finished running
  int nActUnitsFailed_;   // how many have failed

  int dataCollectionLengthSecs_;  // 
  bool useManuallyAssignedDxBandwidth_;
  ObserveActivityOpsBitset actOpsBitset_;  // observe activity operations
  int verboseLevel_;

  ofstream obsSummaryTxtStrm_;   // observation summary output stream
  string archiveFilenamePrefix_; 
  string dataProductsDir_;

  MutexBool stopReceived_;
  MutexBool activityWrappingUp_;

  bool allNonDxComponentsAreDetached_;
  bool processTunedDxsAlreadyRun_;
  bool processAllDataCollCompleteAlreadyRun_;
  bool processAllDoneSendingCwCoherentSignalsAlreadyRun_;

  ACE_Atomic_Op<ACE_Recursive_Thread_Mutex,double> minDxSkyFreqMhz_;
  ACE_Atomic_Op<ACE_Recursive_Thread_Mutex,double> maxDxSkyFreqMhz_;
  mutable ACE_Recursive_Thread_Mutex actTypeMutex_;
  mutable ACE_Recursive_Thread_Mutex diskStatusMsgMutex_;

  ObsSummaryStats combinedObsSummaryStats_;

  Timeout<ObserveActivityImp> tscopeReadyTimeout_;
  Timeout<ObserveActivityImp> ifcReadyTimeout_;
  Timeout<ObserveActivityImp> tsigReadyTimeout_;
  Timeout<ObserveActivityImp> dxTunedTimeout_;
  Timeout<ObserveActivityImp> dataCollectionCompleteTimeout_;
  Timeout<ObserveActivityImp> doneSendingCwCoherentSignalsTimeout_;
  Timeout<ObserveActivityImp> actUnitCompleteTimeout_; 
  Timeout<ObserveActivityImp> stopTimeout_;
  Timeout<ObserveActivityImp> stopActUnitsTimeout_;
  Timeout<ObserveActivityImp> completeDxPrepNotification_;
  Timeout<ObserveActivityImp> pointAntsAndWaitTimeout_;
  Timeout<ObserveActivityImp> actDeleteTimeout_;
  Timeout<ObserveActivityImp> zxLookUpSetiLiveCandidatesTimeout_;

  ACE_Atomic_Op<ACE_Recursive_Thread_Mutex,int> startTime_;
  ACE_Atomic_Op<ACE_Recursive_Thread_Mutex,int> startBaseAccumTime_;
  ACE_Atomic_Op<ACE_Recursive_Thread_Mutex,int> startDataCollTime_;

  int prevActStartTime_;
  TscopeStatusMultibeam tscopeStatusOnTarget_;
  string diskStatusMsg_;
  IfDbOffsetTable *ifAttnDbOffsetTable_;
  int startTimeOffset_;

  double primaryCenterRaRads_;
  double primaryCenterDecRads_;

  double ataTuningSkyFreqMhz_[TSCOPE_N_TUNINGS];
  vector<TscopeBeam> ataBeamsInUse_;

  typedef std::map<string, string> PreludeBeamToAtaTuningNameMap;
  PreludeBeamToAtaTuningNameMap preludeBeamToAtaTuningNameMap_;

  AtaTuningToPreludeBeamsMultiMap ataTuningToPreludeBeamsMultiMap_;

  WorkTask workTask_;

  friend class CreateRecentRfiMaskCmd;
  friend class PrepareFollowupCandidateSignalsCmd;
  friend class CompleteRemainingDxPreparationCmd;
  friend class StopCmd;
  friend class SendCandidatesToDxsForSecondaryProcessingCmd;
  friend class ResolveCandidatesCmd;

  // disable copy construction & assignment.
  // don't define these
  ObserveActivityImp(const ObserveActivityImp& rhs);
  ObserveActivityImp & operator=(const ObserveActivityImp& rhs);

};

 
#endif
