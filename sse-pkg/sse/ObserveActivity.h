/*******************************************************************************

 File:    ObserveActivity.h
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


#ifndef _observeActivity_h
#define _observeActivity_h

//SSE ObserveActivity abstract base class

// This class defines the interface to an ObserveActivity. 
// Subclasses should inherit from the ObserveActivityImp 
// implementation class.

#include "Activity.h"
#include "TargetId.h"
#include <mysql.h>

class ActivityUnit;
class ObsSummaryStats;
class IfcProxy;
class TscopeProxy;
class TestSigProxy;
class ActivityStrategy;
class NssMessage;
class NssDate;
class ActParameters;
class DbParameters;
class DxParameters;

#include "ObserveActivityOps.h"

class ObserveActivity : public Activity
{
 public:

  ObserveActivity(ActivityId_t id,
		  ActivityStrategy* activityStrategy);
  virtual ~ObserveActivity();

  virtual bool start() = 0;
  virtual void stop() = 0;

  // incoming messages from activityUnits (dxs)
  virtual void activityUnitReady(ActivityUnit *activityUnit) = 0;
  virtual void dataCollectionStarted(ActivityUnit *activityUnit) = 0;
  virtual void dataCollectionComplete(ActivityUnit *activityUnit) = 0;
  virtual void signalDetectionStarted(ActivityUnit* activityUnit) = 0;
  virtual void signalDetectionComplete(ActivityUnit* activityUnit) = 0;
  virtual void doneSendingCwCoherentSignals(ActivityUnit* activityUnit) = 0;
  virtual void doneSendingCandidateResults(ActivityUnit* activityUnit) = 0;
  virtual void activityUnitObsSummary(ActivityUnit *activityUnit,
				      ObsSummaryStats &obsSumStats) = 0;
  virtual void activityUnitFailed(ActivityUnit *activityUnit) = 0;
  virtual void activityUnitComplete(ActivityUnit *activityUnit) = 0;

  // incoming messages from other components
  virtual void ifcReady(IfcProxy* ifcProxy) = 0;
  virtual void tscopeReady(TscopeProxy* tscopeProxy) = 0;
  virtual void tscopeCancelTimer(TscopeProxy* tscopeProxy) = 0;
  virtual void testSigReady(TestSigProxy* testSigProxy) = 0;

  virtual void ifcError(IfcProxy * ifcProxy, NssMessage &nssMessage) = 0;
  virtual void tscopeError(TscopeProxy * tscopeProxy, NssMessage &nssMessage) = 0;
  virtual void testSigError(TestSigProxy * testSigProxy, NssMessage &nssMessage) = 0;


  // utilities
  virtual const string getStatus() const = 0;
  virtual const string getActivityName() const = 0;
  virtual string getArchiveFilenamePrefix() const = 0;

  virtual DbParameters& getDbParameters() = 0;
  virtual ActParameters& getActParameters() = 0;
  virtual const DxParameters & getDxParameters() = 0;
  virtual ObserveActivityOpsBitset & getOperations() = 0;

  virtual int getStartTime() const = 0;
  virtual NssDate getStartTimeAsNssDate() const = 0;
  virtual int getPrevActStartTime() const = 0;
  virtual int getBeamNumberForDxName(const string & dxName) = 0;
  virtual TargetId getTargetIdForBeam(int beamNumber) = 0;

  virtual string getSiteName() = 0;
  virtual string prepareObsSummStatsSqlUpdateStmt(
     const ObsSummaryStats & obsSummaryStats) = 0;

  virtual void lookUpTargetCoordinates(
     MYSQL *conn, TargetId targetId,
     double *ra2000Rads, double *dec2000Rads,
     bool *moving) = 0; 
};
 
#endif
