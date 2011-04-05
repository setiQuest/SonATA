/*******************************************************************************

 File:    ConfirmationTask.h
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
// Candidate signal confirmation task
//
// This task controls signal detection for an observation
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/ConfirmationTask.h,v 1.4 2009/02/22 04:48:37 kes Exp $

#ifndef _ConfirmationTaskH
#define _ConfirmationTaskH

#include <bitset>
#include <list>
#include "Activity.h"
#include "CwConfirmationTask.h"
#include "Msg.h"
#include "Partition.h"
#include "PulseConfirmationTask.h"
#include "QTask.h"
#include "Semaphore.h"
#include "Signal.h"
#include "State.h"
#include "Struct.h"

using namespace sonata_lib;

namespace dx {

typedef list<Msg *> SecondaryList;

// done bit masks
enum {
	CWD_CONFIRM_BIT,
	PD_CONFIRM_BIT,
	NBITS_IN_CONFIRM_MASK
};

typedef std::bitset<NBITS_IN_CONFIRM_MASK> ConfirmationMask;

//
// startup arguments
//
struct ConfirmationArgs {
	Queue *controlQ;				// control task queue
	Queue *respQ;					// SSE response queue
	Queue *archiveQ;				// archive task queue

	ConfirmationArgs(): controlQ(0), respQ(0), archiveQ(0) {}
	ConfirmationArgs(Queue *controlQ_, Queue *respQ_, Queue *archiveQ_):
			controlQ(controlQ_), respQ(respQ_), archiveQ(archiveQ_) {}
};

//
// This task controls confirmation of signals selected as
// candidates
//
class ConfirmationTask: public QTask {
public:
	static ConfirmationTask *getInstance();
	~ConfirmationTask();

private:
	static ConfirmationTask *instance;

	int32_t cwCount;					// CW candidate count
	int32_t pulseCount;					// Pulse candidate count
	Activity *activity;					// current activity
	ConfirmationMask stopMask;			// mask to register activity stopped
	Queue *archiveQ;					// archive task queue
	Queue *controlQ;					// control task queue
	Queue *respQ;						// SSE response queue
	Queue *cwQ;							// CW confirmation queue
	Queue *pulseQ;						// Pulse confirmation queue
	Semaphore *cSem;					// candidate semaphore
	SecondaryList secondaryList;		// list of pending secondary candidates

	MsgList *msgList;
	PartitionSet *partitionSet;
	State *state;

	// dependent task info
	CwConfirmationTask *cwTask;
	PulseConfirmationTask *pulseTask;

	CwConfirmationArgs cwConfirmationArgs;
	PulseConfirmationArgs pulseConfirmationArgs;

	// methods
	void extractArgs();
	void createTasks();
	void startTasks();
	void handleMsg(Msg *msg);
	Error handleSecondaryMsg(Msg *msg);

	void startActivity(Msg *msg);
	void stopActivity(Msg *msg, bool stopAll = false);
	void archiveCandidate(Msg *msg);
	void testActivityStopped(Msg *msg);
	void sendActivityStopped(Msg *msg);
	void sendCwComplete();
	void sendPulseComplete();
	void sendConfirmationComplete(Activity *act);
	void startConfirmation();
	void startCwConfirmation();
	void startPulseConfirmation();
	void stopConfirmation();
	void stopCwConfirmation();
	void stopPulseConfirmation();

	void confirmCandidate(Signal *candidate);
	void confirmCw(Signal *cwSig);
	void confirmPulse(Signal *pulseSig);

	// secondary methods
	Error confirmSecondarySignal(Msg *msg, Activity *act);
	void processNextSecondarySignal();
	Error sendCwCoherentResult(Msg *msg, Activity *act);
	Error sendPulseResult(Msg *msg, Activity *act);
	Error endCwCoherentSignals(Msg *msg, Activity *act);

	// hidden
	ConfirmationTask(string name_);

	// forbidden
	ConfirmationTask(const ConfirmationTask&);
	ConfirmationTask& operator=(const ConfirmationTask&);
};

}

#endif
