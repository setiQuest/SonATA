/*******************************************************************************

 File:    ControlTask.h
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
// Control task
//
// This task operates as the system switchboard for ongoing
// activities.  It communicates with the data collection,
// signal detection and confirmation tasks to monitor and
// control system operation.
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/ControlTask.h,v 1.5 2009/03/11 01:36:24 kes Exp $
//
#ifndef _ControlTaskH
#define _ControlTaskH

#include "Args.h"
#include "ArchiveTask.h"
#include "CollectionTask.h"
#include "ConfirmationTask.h"
#include "DetectionTask.h"
#include "Msg.h"
#include "Partition.h"
#include "QTask.h"
#include "SignalClassifier.h"
#include "State.h"
#include "SuperClusterer.h"

using namespace sonata_lib;

namespace dx {

struct ControlArgs {
	Queue *respQ;					// ptr to SSE output queue

	ControlArgs(): respQ(0) {}
	ControlArgs(Queue *respQ_): respQ(respQ_) {}
};

class ControlTask: public QTask {
public:
	static ControlTask *getInstance();
	~ControlTask();

private:
	static ControlTask *instance;

	Queue *archiveQ;					// ptr to archive task queue
	Queue *collectionQ;					// ptr to data collection task queue
	Queue *confirmationQ;				// ptr to confirmation queue
	Queue *detectionQ;					// ptr to detection task queue
	Queue *respQ;						// ptr to SSE output queue
	SignalClassifier classifier;		// signal classifier
#ifdef notdef
	SuperClusterer *superClusterer;		// ptr to super clusterer
#endif

	ArchiveArgs archiveArgs;
	CollectionArgs collectionArgs;
	ConfirmationArgs confirmationArgs;
	DetectionArgs detectionArgs;

	Args *cmdArgs;
	MsgList *msgList;
	PartitionSet *partitionSet;
	State *state;

	ArchiveTask *archiveTask;
	CollectionTask *collectionTask;
	ConfirmationTask *confirmationTask;
	DetectionTask *detectionTask;

	void extractArgs();
	void createTasks();
	void startTasks();
	void handleMsg(Msg *msg);
	Error handleSecondaryMsg(Msg *msg);

	void defineActivity(Msg *msg);

	void tune(Msg *msg);
	void sendTuned(Msg *msg);

	void startRequestedSubchannels(Msg *msg);
	void addRequestedSubchannel(Msg *msg);
	void endRequestedSubchannels(Msg *msg);

	void startFollowupSignals(Msg *msg);
	void addCwFollowupSignal(Msg *msg);
	void addPulseFollowupSignal(Msg *msg);
	void endFollowupSignals(Msg *msg);

	void startActivity(Msg *msg);
	void stopActivity(Msg *msg);
	void stopActivity(Msg *msg, Activity *act);
	void sendCollectionStarted(Msg *msg);
	void sendCollectionComplete(Msg *msg);
	void startDetection(Msg *msg);
	void sendDetectionStarted(Msg *msg);
	void sendDetectionResults(Msg *msg);
	void startConfirmation(Msg *msg);
	void startArchive(Msg *msg);
	void sendConfirmationComplete(Msg *msg);
	void sendArchiveComplete(Msg *msg);
	void requestArchive(Msg *msg);
	void discardArchive(Msg *msg);
	void stopCollection(Msg *msg, int32_t activityId);
	void stopDetection(Msg *msg, int32_t activityId);
	void testActivityStopped(Msg *msg);
	void sendActivityComplete(Msg *msg);

	void clusterSignals(int32_t activityId);
	void classifySignals(int32_t activityId);
	void sendCandidates(int32_t activityId);
	void sendSignals(int32_t activityId);
	void sendBadBands(int32_t activityId);

	// secondary DX messages
	Error addCwCandidate(Msg *msg, Activity *act);
	Error addPulseCandidate(Msg *msg, Activity *act);
	Error endCandidates(Msg *msg, Activity *act);
	Error sendCwCoherentSignal(Msg *msg, Activity *act);
	Error endCwCoherentSignals(Msg *msg, Activity *act);

	// utilities
	bool isPow2(uint32_t value);
	uint32_t pow2(uint32_t value);

	// hidden
	ControlTask(string name_);

	// forbidden
	ControlTask(const ControlTask&);
	ControlTask& operator=(const ControlTask&);
};

}

#endif
