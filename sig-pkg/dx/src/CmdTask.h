/*******************************************************************************

 File:    CmdTask.h
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
// Command processor task
//
// $Header: /home/cvs/nss/sonata-pkg/dx/src/CmdTask.h,v 1.4 2009/02/22 04:48:37 kes Exp $
//
#ifndef _CmdTaskH
#define _CmdTaskH

#include "Activity.h"
//#include "ArchiverTask.h"
#include "CollectionTask.h"
#include "ControlTask.h"
#include "DetectionTask.h"
#include "Msg.h"
#include "Partition.h"
#include "QTask.h"
#include "SseOutputTask.h"
#include "State.h"

using namespace sonata_lib;

namespace dx {

struct CmdArgs {
	Queue *respQ;
	Queue *controlQ;

	CmdArgs(): respQ(0), controlQ(0) {}
	CmdArgs(Queue *respQ_, Queue *controlQ_): respQ(respQ_), controlQ(controlQ_)
			{}
};

class CmdTask: public QTask {
public:
	static CmdTask *getInstance(string s = "");
	~CmdTask();

protected:
	void extractArgs();
	void handleMsg(Msg *msg);

private:
	static CmdTask *instance;

	Queue *controlQ;
	Queue *respQ;

	MsgList *msgList;
	PartitionSet *partitionSet;
	State *state;

	Error handleSecondaryMsg(Msg *msg);
	void sendIntrinsics(Msg *msg);
	void configure(Msg *msg);
	void setPermRfiMask(Msg *msg);
	void setBirdieMask(Msg *msg);
	void setRcvrBirdieMask(Msg *msg);
	void setRecentRfiMask(Msg *msg);
	void setTestSignalMask(Msg *msg);
	void setSessionParameters(Msg *msg);
	void sendStatus(Msg *msg);
	void defineActivity(Msg *msg);
	void sendScienceData(Msg *msg);
	void startRequestedSubchannels(Msg *msg);
	void sendRequestedSubchannel(Msg *msg);
	void endRequestedSubchannels(Msg *msg);
	void startFollowUpSignals(Msg *msg);
	void addFollowUpCwSignal(Msg *msg);
	void addFollowUpPulseSignal(Msg *msg);
	void endFollowUpSignals(Msg *msg);
	void startActivity(Msg *msg);
	void stopActivity(Msg *msg);
	void requestArchive(Msg *msg);
	void discardArchive(Msg *msg);
	void shutdown(Msg *msg);
	void restart(Msg *msg);

	Error tune(Activity *act);

	// secondary-specific methods
	Error startCandidates(Msg *msg, Activity *act);
	Error addCwCandidate(Msg *msg, Activity *act);
	Error addPulseCandidate(Msg *msg, Activity *act);
	Error endCandidates(Msg *msg, Activity *act);
	Error startCwCoherentSignals(Msg *msg, Activity *act);
	Error sendCwCoherentSignal(Msg *msg, Activity *act);
	Error endCwCoherentSignals(Msg *msg, Activity *act);

	// hidden
	CmdTask(string tname_);

	// forbidden
	CmdTask(const CmdTask&);
	CmdTask& operator=(const CmdTask&);
};

}

#endif
