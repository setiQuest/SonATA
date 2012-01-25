/*******************************************************************************

 File:    ObserveActivityImp.cpp
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
 * Observe Activity Implementation.
 * Controls a SETI observation, commanding & coordinating hardware components
 * (particularly dxs, via ActivityUnits),  updating the database & logs, etc. 
 */

#include "ace/OS.h"
#include "ObserveActivityImp.h"

#include "ActivityException.h"
#include "ActivityStrategy.h"
#include "ActivityUnitImp.h"
#include "ActParameters.h"
#include "AtaInformation.h"
#include "CmdPattern.h"
#include "ChannelizerProxy.h"
#include "DebugLog.h"
#include "ExpectedNssComponentsTree.h"
#include "IfcNames.h"
#include "IfcProxy.h"
#include "IfDbOffsetTable.h"
#include "MinMaxDxSkyFreqMhz.h"
#include "MsgSender.h"
#include "MysqlQuery.h"
#include "NssComponentTree.h"
#include "NssParameters.h"
#include "OffPositions.h"
#include "DxParameters.h"
#include "DxProxy.h"
#include "Spacecraft.h"
#include "SseArchive.h"
#include "SseAstro.h"
#include "SseCommUtil.h"
#include "SseMessage.h"
#include "SseDxMsg.h"
#include "SseSystem.h"
#include "SseTscopeMsg.h"
#include "sseVersion.h"
#include "TargetPosition.h"
#include "TestSigNames.h"
#include "TestSigProxy.h"
#include "TscopeProxy.h"

//Added for channelizer - beaformer bfinit
#include "ChannelizerProxy.h"
#include "ChannelizerList.h"
#include <memory>

using namespace std;

static const char * IfAttnDbOffsetFilename = "ifAttnDbOffset.cfg";
static const double DxHalfFrameLengthSecs = 0.80;
static const int PrintPrecision = 9;  // MilliHz
static const char * PrimaryBeam = "primary";

// This activityLogStream stuff is static since it's shared 
// across activities (and therefore threads)
static ofstream & getActivityLogStream();
static ACE_Recursive_Thread_Mutex activityLogStreamMutex; 



ObserveActivityImp::TargetCoordinates::TargetCoordinates()
	:
		ra2000Rads(0.0),
		dec2000Rads(0.0)
{
}

ObserveActivityImp::TargetInfo::TargetInfo()
	:
		targetId(-1),
		tuningSkyFreqMhz(-1)
{
}

/*
   Create a new ACE_Message_Block that contains
   a pointer to a new CmdPattern command,
   and put it in the the workTask queue.
   The command pattern is expected to take a "this" argument
   in the constructor.
   */
#define PutCmdPatternInWorkTaskQueue(cmdPatternObject) \
	workTask_.putq(new ACE_Message_Block((const char *) \
				new (cmdPatternObject)(this),sizeof(const char *)))

#define PutThreadExitMessageInWorkTaskQueue \
	workTask_.putq(new ACE_Message_Block(0, ACE_Message_Block::MB_STOP))


class CreateRecentRfiMaskCmd : public CmdPattern
{
	public:
		CreateRecentRfiMaskCmd(ObserveActivityImp *obsAct)
			: obsAct_(obsAct)
		{}

		virtual void execute(const string & cmdArgs)
		{
			obsAct_->sendRecentRfiMask();
		}

	private:
		ObserveActivityImp *obsAct_;
};

class PrepareFollowupCandidateSignalsCmd : public CmdPattern
{
	public:
		PrepareFollowupCandidateSignalsCmd(ObserveActivityImp *obsAct)
			: obsAct_(obsAct)
		{}

		virtual void execute(const string & cmdArgs)
		{
			obsAct_->prepareFollowupCandidateSignals();
		}

	private:
		ObserveActivityImp *obsAct_;
};

class CompleteRemainingDxPreparationCmd : public CmdPattern
{
	public:
		CompleteRemainingDxPreparationCmd(ObserveActivityImp *obsAct)
			: obsAct_(obsAct)
		{}

		virtual void execute(const string & cmdArgs)
		{
			/*
			   Set up a timer callback to get the reactor 
			   to execute this code, to avoid thread conflicts.

TBD: make the called procedure thread safe
and let the worktask handle it.
*/

			int waitTimeSecs = 1; 
			obsAct_->completeDxPrepNotification_.startTimer(
					waitTimeSecs,
					obsAct_, &ObserveActivityImp::completeRemainingDxPreparation,
					obsAct_->verboseLevel_);
		}

	private:
		ObserveActivityImp *obsAct_;
};

class StopCmd : public CmdPattern
{
	public:
		StopCmd(ObserveActivityImp *obsAct)
			: obsAct_(obsAct)
		{}

		virtual void execute(const string & cmdArgs)
		{
			/*
			   Register timer handlers with the ace reactor
			   that do the remaining work of stopping an activity.
			   Using a timed callback in this way gives the dxs a chance
			   to respond to the stop command before the activity terminates.
			   More importantly, it synchronizes the stop command 
			   so that it cannot occur in the middle of any of the
			   other activity methods that are called due to messages arriving
			   at the Reactor.

			   Note that the dxs may all disconnect in response to
			   the stopActivityUnits command, causing the activity to terminate
			   before the stopActivity command executes.  This should not be
			   a problem.
			   */

			/* 
			   Stop the activity units (and their dxs)
			   giving them a chance to wrap up before the activity
			   is terminated via the following 'stop'.
			   */
			int stopActUnitsWaitTimeSecs = 1; 
			obsAct_->stopActUnitsTimeout_.startTimer(
					stopActUnitsWaitTimeSecs,
					obsAct_, &ObserveActivityImp::stopActivityUnits,
					obsAct_->verboseLevel_);

			/*
			   Set the stop timer delay long enough to give the dxs a chance
			   to check in but not so long that it feels unresponsive to the user.
			   */
			int stopActWaitTimeSecs = 2; 
			obsAct_->stopTimeout_.startTimer(
					stopActWaitTimeSecs,
					obsAct_, &ObserveActivityImp::stopActivity,
					obsAct_->verboseLevel_);
		}

	private:
		ObserveActivityImp *obsAct_;
};


class SendCandidatesToDxsForSecondaryProcessingCmd : public CmdPattern
{
	public:
		SendCandidatesToDxsForSecondaryProcessingCmd(ObserveActivityImp *obsAct)
			: obsAct_(obsAct)
		{}

		virtual void execute(const string & cmdArgs)
		{
			obsAct_->sendCandidatesToDxsForSecondaryProcessing();
		}

	private:
		ObserveActivityImp *obsAct_;
};


class ResolveCandidatesCmd : public CmdPattern
{
	public:
		ResolveCandidatesCmd(ObserveActivityImp *obsAct)
			: obsAct_(obsAct)
		{}

		virtual void execute(const string & cmdArgs)
		{
			obsAct_->resolveCandidatesBasedOnSecondaryProcessingResults();
		}

	private:
		ObserveActivityImp *obsAct_;
};



ObserveActivityImp::WorkTask::WorkTask()
{

}

int ObserveActivityImp::WorkTask::start()
{
	const int numberOfThreads(1);
	int result = this->activate(THR_NEW_LWP | THR_DETACHED, numberOfThreads);

	return result;
}

/*
   This ACE_Task method waits for ACE_Message_Blocks, and then
   executes the CmdPattern that is pointed to by the block's message
   data section.
   Both the CmdPattern and ACE_Message_Block are
   deleted when execution completes.
   Thread exits when an ACE_Message_Block with a type of MB_STOP
   is received.

*/
int ObserveActivityImp::WorkTask::svc(void)
{
	ACE_Message_Block *msgBlock;
	const string methodName("ObserveActivityImp::WorkTask::svc");

	while (true)
	{
		// Get the next ACE_Message_Block.  This will sleep
		// until the next message is available.

		if (this->getq(msgBlock) == -1)
		{
			SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, 
					SSE_MSG_ACT_FAILED, 
					SEVERITY_ERROR,
					"svc() getq failed",
					__FILE__, __LINE__);

			return -1;
		}

		if (msgBlock->msg_type() == ACE_Message_Block::MB_STOP)
		{
			break;
		}

		// Get the cmd pattern the block points to, in its message data.
		char *msgData(msgBlock->base());
		CmdPattern *cmd = reinterpret_cast<CmdPattern *>(msgData);

		try {
			cmd->execute("");
		}
		catch (SseException &except)
		{
			SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID, except.code(),
					except.severity(), except.descrip(), 
					except.sourceFilename(), except.lineNumber());
		}
		catch(...)
		{
			stringstream strm;
			strm << "Caught unexpected exception in " << methodName << endl;
			SseMessage::log(MsgSender, NSS_NO_ACTIVITY_ID,
					SSE_MSG_EXCEPTION, SEVERITY_ERROR,
					strm.str(), __FILE__, __LINE__);
		}

		delete(cmd);
		msgBlock->release();
	}

	// SseArchive::SystemLog() << "****** svc thread exited ******" << endl;

	return 0;
}



ObserveActivityImp::ObserveActivityImp(ActivityId_t id,
		ActivityStrategy* activityStrategy, 
		const string &activityName,
		NssComponentTree *nssComponentTree,
		const NssParameters& nssParameters,
		const ObserveActivityOpsBitset &actOpsBitset,
		const DxOpsBitset &dxOpsBitset,
		int verboseLevel) :
	ObserveActivity(id, activityStrategy),
	activityName_(activityName),
	nssComponentTree_(nssComponentTree),
	actParameters_(*nssParameters.act_),
	tscopeParameters_(*nssParameters.tscope_),
	ifc1Parameters_(*nssParameters.ifc1_),
	ifc2Parameters_(*nssParameters.ifc2_),
	ifc3Parameters_(*nssParameters.ifc3_),
	testSig1Parameters_(*nssParameters.tsig1_),
	testSig2Parameters_(*nssParameters.tsig2_),
	testSig3Parameters_(*nssParameters.tsig3_),
	schedulerParameters_(*nssParameters.sched_),
	dbParameters_(*nssParameters.db_),
	dbParametersForWorkThread_(*nssParameters.db_),
	dbParametersForTerminate_(*nssParameters.db_),
	dxParameters_(*nssParameters.dx_),
	dxActParameters_(nssParameters.dx_->getDxActParamStruct()),
	siteName_(nssParameters.act_->getSiteName()),
	expectedTree_(nssComponentTree->getExpectedNssComponentsTree()),
	nTscopesStarted_(0),
	nTscopesReady_(0),
	nTestSigsStarted_(0),
	nTestSigsReady_(0),
	nIfcsStarted_(0),
	nIfcsReady_(0),
	nActUnitsStarted_(0),
	nActUnitsStillWorking_(0),
	nActUnitsReady_(0),
	nActUnitsDataCollectionStarted_(0),
	nActUnitsDataCollectionComplete_(0),
	nActUnitsSignalDetectionStarted_(0),
	nActUnitsSignalDetectionComplete_(0),
	nActUnitsDoneSendingCwCoherentSignals_(0),
	nActUnitsDoneSendingCandidateResults_(0),
	nActUnitsDone_(0),
	nActUnitsFailed_(0),
	dataCollectionLengthSecs_(nssParameters.dx_->getDataCollectionLengthSecs()),
	useManuallyAssignedDxBandwidth_(nssParameters.dx_->useManuallyAssignedDxBandwidth()),
	actOpsBitset_(actOpsBitset),
	verboseLevel_(verboseLevel),
	allNonDxComponentsAreDetached_(false),
	processTunedDxsAlreadyRun_(false),
	processAllDataCollCompleteAlreadyRun_(false),
	processAllDoneSendingCwCoherentSignalsAlreadyRun_(false),
	minDxSkyFreqMhz_(-1),
	maxDxSkyFreqMhz_(-1),
	tscopeReadyTimeout_("tscope ready"),
	ifcReadyTimeout_("ifc ready"),
	tsigReadyTimeout_("test sig gens ready"),
	dxTunedTimeout_("dx tuned"),
	dataCollectionCompleteTimeout_("data coll complete"),
	doneSendingCwCoherentSignalsTimeout_("done sending CW Coherent signals"),
	actUnitCompleteTimeout_("act unit complete"),
	stopTimeout_("stop"),
	stopActUnitsTimeout_("stopActUnits"),
	completeDxPrepNotification_("complete dx prep"),
	pointAntsAndWaitTimeout_("point ants and wait"),
	actDeleteTimeout_("delete activity"),
	startTime_(0),
	prevActStartTime_(0),
	ifAttnDbOffsetTable_(0),
	startTimeOffset_(0),
	primaryCenterRaRads_(-1),
	primaryCenterDecRads_(-1)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": "
			<< "ObserveActivityImp::ctor()" << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< getActivityType() << " Act Created " << endl;

	stopReceived_.set(false);
	activityWrappingUp_.set(false);

	prepareDataProductsArchive();

	checkAvailableArchiveDiskSpace(getDataProductsDir());

	createObsSummaryFileStream();

	writeObsSummaryHeader(nssParameters);

	loadIfAttnDbOffsetTable();

	startTimeOffset_ = calculateStartTimeOffset();

	if (schedulerParameters_.useMultipleTargets() && useDx())
	{
		// For OFFs leave the activity in single
		// target mode, even if the parameters have
		// multitarget enabled.  No secondary candidate
		// processing is required.

		if (!isOffObservation())
		{
			actOpsBitset_.set(MULTITARGET_OBSERVATION);
		}
	}

	adjustDxActivityParameters(dxActParameters_, dxOpsBitset);

	getObsSummaryTxtStrm() << "Dx: " << endl
		<< dxActParameters_ << endl;


	for (int i=0; i<TSCOPE_N_TUNINGS; ++i)
	{
		ataTuningSkyFreqMhz_[i] = 0.0; 
	}

	workTask_.start();
}

ObserveActivityImp::~ObserveActivityImp()
{
	delete ifAttnDbOffsetTable_;

	deleteAllActivityUnits();

	delete nssComponentTree_;
}

const string ObserveActivityImp::getActivityName() const
{
	return activityName_;
}

const string ObserveActivityImp::getActivityType() const
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(actTypeMutex_);

	return actParameters_.getActivityType();
}

int ObserveActivityImp::getPrevActStartTime() const
{
	return prevActStartTime_;
}


int ObserveActivityImp::getStartTime() const
{
	return startTime_.value();
}

NssDate ObserveActivityImp::getStartTimeAsNssDate() const
{
	NssDate nssDate;
	nssDate.tv_sec = getStartTime();

	return nssDate;
}


DbParameters &ObserveActivityImp::getDbParameters()
{
	return dbParameters_;
}


// deliberately non-const to allow modification
ObserveActivityOpsBitset & ObserveActivityImp::getActOpsBitset()
{
	return actOpsBitset_;
}

ostream & ObserveActivityImp::getObsSummaryTxtStrm()
{
	return obsSummaryTxtStrm_;
}

NssComponentTree * ObserveActivityImp::getNssComponentTree()
{
	return nssComponentTree_;
}

double ObserveActivityImp::getMinDxSkyFreqMhz() const
{
	return minDxSkyFreqMhz_.value();
}

double ObserveActivityImp::getMaxDxSkyFreqMhz() const
{
	return maxDxSkyFreqMhz_.value();
}


bool ObserveActivityImp::useManuallyAssignedDxBandwidth()
{
	return useManuallyAssignedDxBandwidth_;
}

TestSigParameters & ObserveActivityImp::getTestSigParameters(const string &testSigName)
{
	if (testSigName == TestSig1Name)
	{
		return testSig1Parameters_;
	}
	else if (testSigName == TestSig2Name)
	{
		return testSig2Parameters_;
	}
	else if (testSigName == TestSig3Name)
	{
		return testSig3Parameters_;
	}
	else 
	{
		stringstream strm;
		strm << "unknown testSig, can't get parameters for: " << testSigName << endl;
		throw SseException(strm.str(), __FILE__, __LINE__,
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

}

ActParameters & ObserveActivityImp::getActParameters()
{
	return actParameters_;
}

const DxParameters & ObserveActivityImp::getDxParameters()
{
	return dxParameters_;
}


ObserveActivityOpsBitset & ObserveActivityImp::getOperations()
{
	return actOpsBitset_;
}

string ObserveActivityImp::getSiteName()
{
	return siteName_;
}


static int beamNameToNumber(const string &beamName)
{
	// Extract the beam number from the end of the beam name,
	// which is expected to be of the form "beamN" where N is the
	// beam number.

	string::size_type startOfBeamNumber(beamName.find_first_of("0123456789"));
	Assert(startOfBeamNumber != string::npos);

	string beamNumberString(beamName.substr(startOfBeamNumber));

	// this may throw an SseException error
	int beamNumber = SseUtil::strToInt(beamNumberString);

	return beamNumber;
}


// return the beam number associated with the specified dx.
// If the dx name is not found, or if there is an error extracting
// the beam number,  then -1 is returned.

int ObserveActivityImp::getBeamNumberForDxName(const string & dxName)
{
	int beamNumber(-1);

	string beamName = expectedTree_->getBeamForDx(dxName);
	if (beamName != "")
	{
		// Extract the beam number from the end of the beam name.
		try {
			beamNumber = beamNameToNumber(beamName);
		}
		catch(...)
		{
			// do nothing, use the default
			// TBD, log an error or warning?
		}
	}

	return beamNumber;
}

// return the target associated with the specified beam.
// If the beam is not found, then 0 is returned for the targetid.

TargetId ObserveActivityImp::getTargetIdForBeam(int beamNumber)
{
	TargetId targetId(0);

	try
	{
		stringstream beamName;

		// TBD find better way to select beam than literal string name
		beamName << "beam" << beamNumber;  

		targetId = actParameters_.getTargetIdForBeam(beamName.str());
	}
	catch (SseException)
	{
		//  beam not found, use the default value above
	}

	return targetId;

}



// determine the list of components that are to be used,
// which may be a subset of those available, depending
// on the user request

void ObserveActivityImp::determineDesiredComponents()
{
	if (useIfc())
	{
		// for each requested beam, get the associated components
		vector<string> beamsToUse(schedulerParameters_.getBeamsToUse());

		if (beamsToUse.size() == 0)
		{
			throw SseException("No beams were specified\n", __FILE__, __LINE__,
					SSE_MSG_ACT_FAILED, SEVERITY_ERROR);
		}

		stringstream beamsToUseStream;
		for (vector<string>::iterator it = beamsToUse.begin();
				it != beamsToUse.end(); ++it)
		{
			const string & beamName = *it;
			beamsToUseStream << beamName << " ";

			vector<string> beamSublist;
			beamSublist.push_back(beamName);

			// Ifcs
			IfcList ifcsForBeam = getNssComponentTree()->getIfcsForBeams(beamSublist);
			if (ifcsForBeam.size() == 0)
			{
				throw SseException(
						string("No ifc available for beam: ") + beamName + "\n",
						__FILE__, __LINE__, SSE_MSG_MISSING_IFC, SEVERITY_ERROR);
			}
			ifcList_.insert(ifcList_.end(), ifcsForBeam.begin(), 
					ifcsForBeam.end());
			// there can be more than one beam per ifc so eliminate duplicates
			ifcList_.sort();
			ifcList_.unique();

			if (useDx())
			{
				// Dxs
				DxList dxsForBeam = getNssComponentTree()->getDxsForBeams(beamSublist);
				if (dxsForBeam.size() == 0)
				{
					throw SseException(
							string("No dxs available for beam: ") + beamName + "\n",
							__FILE__, __LINE__, SSE_MSG_MISSING_DX, SEVERITY_ERROR);
				}
				dxList_.insert(dxList_.end(), dxsForBeam.begin(), dxsForBeam.end());
			}
		}


		SseArchive::SystemLog() << "Act " << getId() << ": " 
			<< "requested beams: " 
			<< beamsToUseStream.str() << endl;
	}
	else
	{
		if (useDx())
		{
			// Not using any ifcs, so use all available dxs.

			// copy the dx list
			dxList_ = nssComponentTree_->getDxs();
		}
	}

	if (useDx() && dxList_.empty())
	{
		throw SseException(
				"None of the requested dxs are available for the activity\n",
				__FILE__, __LINE__, SSE_MSG_MISSING_DX, SEVERITY_ERROR);
	}

	if (useTscope())
	{
		// Use all available tscopes.  It's expected that in most cases 
		// there will only be one -- all the beams we plan to use should
		// be accessible via that telescope.
		tscopeList_ = nssComponentTree_->getTscopes();

		if (tscopeList_.size() == 0)
		{
			throw SseException(
					"No tscope(s) available\n",
					__FILE__, __LINE__, SSE_MSG_MISSING_TSCOPE, SEVERITY_ERROR);
		}
	}

	// check for test gen 
	if (useTestgen() && getNssComponentTree()->getTestSigs().empty())
	{
		throw SseException("No tsigs available for activity\n",
				__FILE__, __LINE__,
				SSE_MSG_MISSING_TSIG, SEVERITY_ERROR);
	}

}


// Check the available disk space on the archive disk.
// against the warning & error limits.
// Throws an exception if the error limit is exceeded.

void ObserveActivityImp::checkAvailableArchiveDiskSpace(const string &disk)
{
	const double diskPercentUsed = SseUtil::getDiskPercentUsed(disk);

	stringstream strm;
	strm.precision(1);  // show N places after the decimal
	strm.setf(std::ios::fixed); // show all decimal places up to precision

	if (diskPercentUsed > actParameters_.getDiskPercentFullErrorLimit())
	{
		strm << "Archive disk full error limit exceeded: archive disk "
			<< "(" << dataProductsDir_ << ")"
			<< " is " 
			<< diskPercentUsed << "% full, error limit is "
			<< actParameters_.getDiskPercentFullErrorLimit()
			<< "%." << endl;

		throw SseException(strm.str(), __FILE__, __LINE__,
				SSE_MSG_DISK_FULL_ERR, SEVERITY_ERROR);

	}

	if (diskPercentUsed > actParameters_.getDiskPercentFullWarningLimit())
	{
		strm << "Archive disk full warning limit exceeded: archive disk "
			<< "(" << dataProductsDir_ << ")"
			<< " is " 
			<< diskPercentUsed << "% full, warning limit is "
			<< actParameters_.getDiskPercentFullWarningLimit()
			<< "%." << endl;

		SseMessage::log(MsgSender, getId(),
				SSE_MSG_DISK_FULL_WARN, SEVERITY_WARNING, 
				strm.str(), __FILE__, __LINE__);

		strm.str("");
		strm << " ** Warning: disk " << diskPercentUsed << "% full **";
		setDiskStatusMsg(strm.str());

	}


}

// create the archive dir & predefine a filename
// prefix for the data product files
void ObserveActivityImp::prepareDataProductsArchive()
{
	dataProductsDir_ = SseArchive::prepareDataProductsDir(getId());

	string prefix = dataProductsDir_;

	// prefix the filename with the current date/time
	// to make it unique
	prefix += SseUtil::currentIsoDateTimeSuitableForFilename();
	prefix += ".act" + SseUtil::intToStr(getId());  // activity Number
	prefix += "." + getActivityType();

	archiveFilenamePrefix_ = prefix;

}


// return the archive dir & filename prefix for archive files
string ObserveActivityImp::getDataProductsDir()
{
	return dataProductsDir_;
}


// return the archive dir & filename prefix for archive files
string ObserveActivityImp::getArchiveFilenamePrefix() const
{
	return archiveFilenamePrefix_;
}


void  ObserveActivityImp::loadIfAttnDbOffsetTable()
{
	ifAttnDbOffsetTable_ = new IfDbOffsetTable(
			SseSystem::getSetupDir() +
			"/" + IfAttnDbOffsetFilename);
}

/*
   Estimate activity overall percent completion
   since time of start of data collection.
   */
double ObserveActivityImp::percentComplete() const
{
	double percentComplete(0);

	if (startTime_ > 0)
	{
		time_t currentTime(time(NULL));   
		int elapsedTimeSinceStartSecs(currentTime - startTime_.value());

		if (elapsedTimeSinceStartSecs > 0)
		{
			/* estimate the overall act time based on data coll length */
			const double dataCollLenToActLenFactor(1.5);
			double estimatedActLengthSecs = dataCollectionLengthSecs_ *
				dataCollLenToActLenFactor;
			Assert(estimatedActLengthSecs > 0);

			percentComplete = (static_cast<double>(elapsedTimeSinceStartSecs) / 
					estimatedActLengthSecs) * 100;
		}
	}

	return percentComplete;
}

/*
   Get status.  All data items referred to are thread safe for reading.
   */
const string ObserveActivityImp::getStatus() const
{
	stringstream strm;
	strm.precision(1);  // show N places after the decimal
	strm.setf(std::ios::fixed); // show all decimal places up to precision

	strm << "Act " << getId() 
		<< ": " << getActivityType() 
		<< " | #DXs: " << actUnitStillWorkingListMutexWrapper_.listSize()
		<< " | Min/Max(MHz):" << getMinDxSkyFreqMhz() << "/" << getMaxDxSkyFreqMhz() 
								 << " | Len:" << dataCollectionLengthSecs_ << "s";

	strm.precision(0);
	strm << " | " << percentComplete() << "% done"
		<< " | " << statusStage_.getString();  // eg, "data coll start"

	strm << " " << getDiskStatusMsg();

	strm << endl;

	return strm.str();
}


void ObserveActivityImp::checkModesThatRequireDatabaseToBeOn()
{
	// For followups, check the scheduler params rather than the act 
	// params here,
	// so that the error is caught before the first activity starts,
	// rather than the first actual followup activity.

	if (schedulerParameters_.followupEnabled())
	{
		throw SseException(
				"Database must be turned on for follow up activities\n",
				__FILE__, __LINE__, SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	if (isMultitargetObservation())
	{
		throw SseException(
				"Database must be turned on for multitarget observations\n",
				__FILE__, __LINE__, SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	if (createRecentRfiMaskEnabled())
	{
		throw SseException(
				"Database must be turned on to create recent RFI Masks\n",
				__FILE__, __LINE__, SSE_MSG_DBERR, SEVERITY_ERROR);
	}

}

// Start the observation. 
// Returns true on success.

bool ObserveActivityImp::start()
{
	try 
	{
		if (! dbParameters_.useDb())
		{
			checkModesThatRequireDatabaseToBeOn();
		}

		determineDesiredOperations();

		determineDesiredComponents();

		if (forceArchivingAroundCenterTuning())
		{
			prepareForForcedArchivingAroundCenterTuning();
		}

		if (useDx())
		{
			verifyMinMaxDxSkyFreqs();
		}      

		findAtaTuningsForPreludeBeamsInUse(ataTuningToPreludeBeamsMultiMap_);
		determineAtaTuningFreqsForPreludeBeamsInUse(ataTuningToPreludeBeamsMultiMap_);

		updateStatus(ObserveActivityStatus::ACTIVITY_STARTED); 

		if (followUpObservationEnabled())
		{
			SseArchive::SystemLog() << "Act " << getId() << ": " 
				<< "Follow up of act " 
				<< actParameters_.getPreviousActivityId() << endl; 

			getPreviousActivityInfoFromDb(actParameters_.getPreviousActivityId());
		}

		startComponents();

		if (actParameters_.emailActStatus())
		{
			mailStartingObservationMessage();
		}

	} 
	catch (SseException & except)
	{
		stringstream strm;

		strm << "activity start() failed: " << except.descrip();
		SseMessage::log(MsgSender, getId(),
				except.code(), except.severity(), strm.str(),
				except.sourceFilename(), except.lineNumber());

		terminateActivity("Could not start activity");

		return false;
	}

	return true;

}

void ObserveActivityImp::prepareForForcedArchivingAroundCenterTuning()
{
	/* Make sure scheduler is in multitarget mode.  This is required
	   for the dx to process the 'fake' secondary candidates that will be 
	   created.
	   */
	if (! schedulerParameters_.useMultipleTargets())
	{   
		throw SseException(
				"Scheduler multitarget mode must be turned on for forced archiving activity type\n",
				__FILE__, __LINE__,  SSE_MSG_INVALID_PARMS, SEVERITY_ERROR);
	}

	// TBD: Make sure to automatically enable archiving of all candidates, since the
	// 'fake' candidates created in this mode will not confirm.
	// For now just check:

	if (actParameters_.getCandidateArchiveOption() != 
			ActParameters::ARCHIVE_ALL_CANDIDATES)
	{
		throw SseException(
				"Activity candidate archiving must be set to 'all' for the forced archiving activity type\n",
				__FILE__, __LINE__,  SSE_MSG_INVALID_PARMS, SEVERITY_ERROR);
	}

}


void ObserveActivityImp::verifyMinMaxDxSkyFreqs()
{
	minDxSkyFreqMhz_ = MinDxSkyFreqMhz(dxList_);
	maxDxSkyFreqMhz_ = MaxDxSkyFreqMhz(dxList_);

	if (minDxSkyFreqMhz_ < 0 || maxDxSkyFreqMhz_ < 0)
	{
		stringstream strm;

		strm << "dx sky freq is < 0 for all requested dxs,"
			<< " marking them all as 'not to be used'"
			<< endl;

		throw SseException(strm.str(), __FILE__, __LINE__,
				SSE_MSG_INVALID_SKY_FREQ, SEVERITY_ERROR);
	}
}


// mail out a message announcing the start of
// another observation

void ObserveActivityImp::mailStartingObservationMessage()
{
	string toList(actParameters_.getEmailActStatusAddressList());
	string subject("Current Targets");

	stringstream body; 
	body << "Act: " << getId() << "\n"
		<< "Targets: " << getTargetIdsForAllBeamsInUse() << "\n"
		<< "Time: " << SseUtil::currentIsoDateTime() << endl
		<< "Obs Length: " << dataCollectionLengthSecs_ << " seconds" << endl;

	SseUtil::mailMsg(subject, toList, body.str());

}

void ObserveActivityImp::stop()
{
	// only accept stop command once.
	// don't process a stop if the activity is already wrapping up
	// (ie, in the process of terminating or completing)

	if (! stopReceived_.get() && ! activityWrappingUp_.get())   
	{
		stopReceived_.set(true);

		SseArchive::SystemLog() << "Act " << getId() 
			<< ": Stop command received" << endl;

		updateStatus(ObserveActivityStatus::ACTIVITY_STOPPING); 

		/*
		   Put the stop command on the work task queue, so that
		   it will only be executed when the task's thread is idle,
		   to allow that thread to wrap up cleanly.
		   */

		PutCmdPatternInWorkTaskQueue(StopCmd);

		// don't do anything after this point since the
		// activity will terminate

	}
}


void ObserveActivityImp::stopActivity()
{
	updateStatus(ObserveActivityStatus::ACTIVITY_STOPPED); 

	terminateActivity("Stop command received");
}

/*
   Assumes that this method will only be called
   by the scheduler or reactor threads, not by the
   work task thread, for thread safety.
   */
void ObserveActivityImp::stopActivityUnits()
{
	// forward stop message to each activity unit on the list

	ActUnitList actUnitList(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	ActUnitList::iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it) 
	{
		ActivityUnit *actUnit = *it;

		VERBOSE2(verboseLevel_, "Act " << getId() << ": "
				<< "ObserveActivityImp::stop --> sending stop to actunit "
				<< actUnit->getId() << endl;);

		actUnit->stop(dbParameters_);
	}
}

// Have the activity delete itself.
// Use a timer to delay long enough to
// avoid any race conditions that may occur
// during cleanup.

void ObserveActivityImp::destroy()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "ObserveActivityImp:destroy called. "
			<< " Starting delete timer..." << endl;);

	// wait long enough for activity to clean up
	const int waitTimeSecs = 10;

	actDeleteTimeout_.setExpectToBeDeletedAfterTimeoutMethodIsCalled(true);

	actDeleteTimeout_.startTimer(waitTimeSecs, 
			this, &ObserveActivityImp::deleteSelf,
			verboseLevel_);
}


void ObserveActivityImp::deleteSelf()
{
	delete this;
}


// ---------incoming messages from actUnits -------

void 
ObserveActivityImp::
activityUnitReady(ActivityUnit *actUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: activityUnitReady" << endl;);

	if (activityWrappingUp_.get())
	{
		return;
	}

	// Accumulate activityUnitReady messages from all
	// activity units.   When all report in, then calculate the
	// start time & send it to all units.

	// count units that report in.
	nActUnitsReady_++;

	// keep track of which ones reported in.
	actUnitReadyList_.push_back(actUnit);   

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"nActUnitsReady=" << nActUnitsReady_ << 
			" nActUnitsStillWorking=" << nActUnitsStillWorking_ << endl;);

	if (nActUnitsReady_ == nActUnitsStillWorking_)
	{
		dxTunedTimeout_.cancelTimer();

		SseArchive::SystemLog()  << "Act " << getId() << ": "
			<< nActUnitsReady_ 
			<< " Activity Unit(s) (dxs) ready " << endl;

		processTunedDxs();

	}
}

void ObserveActivityImp::processTunedDxs()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: processTunedDxs" << endl;);

	if (! processTunedDxsAlreadyRun_)
	{
		processTunedDxsAlreadyRun_ = true;

		beginDxPreparation();

	}
}

/*
   Set up any potentially time-consuming dx preparations
   by sending messages to the internal work thread
   to execute them, rather than tying up the calling 
   thread (presumably the ACE reactor).  All methods
   eventually called must be thread safe.
   */
void ObserveActivityImp::beginDxPreparation()
{
	// send recent rfi mask
	if (createRecentRfiMaskEnabled())
	{
		PutCmdPatternInWorkTaskQueue(CreateRecentRfiMaskCmd);
	}

	// Prepare follow up candidate signals.
	// Do this before the start time is calculated, since it may take
	// a nontrivial amount of time.
	if (followUpObservationEnabled())
	{
		PutCmdPatternInWorkTaskQueue(PrepareFollowupCandidateSignalsCmd);
	}

	PutCmdPatternInWorkTaskQueue(CompleteRemainingDxPreparationCmd);
}

/*
   Assume this method will be executed via the Reactor (not the workTask 
   thread) with regard to any multithread interactions.
   */
void ObserveActivityImp::completeRemainingDxPreparation()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": "
			<< "ObserveActivityImp::completeRemainingDxPreparation" << endl;);

	startTime_ = calculateStartTime();

	if (followUpObservationEnabled())
	{
		int nSignalsSent = 
			sendFollowupCandidateSignals(getStartTimeAsNssDate());
		if (nSignalsSent == 0)
		{
			terminateActivity("No signals found for followup");
			return;
		}
	}

	sendStartTime(getStartTimeAsNssDate());

	updateStatus(ObserveActivityStatus::PENDING_DATA_COLLECTION);

	startActUnitWatchdogTimers();


	/* Record the parameters in the database.
	   By this time all of the rcvr, dx, etc.
	   tunings are already calculated.
	   */
	storeParametersInDb();

}


/* 
   One or more of the dxs did not report in as expected.
   Shutdown the associated activity units.
   If there's at least one dx still viable,
   then continue on with the methodToCallOnSuccess.
   */
void ObserveActivityImp::handleDxTimeout(
		const string & expectedMessageDescrip,
		const ActUnitList & actUnitsReportedIn,
		void (ObserveActivityImp::*methodToCallOnSuccess)())
{
	ActUnitList actUnitsStarted(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	int nDxsReportedIn = actUnitsReportedIn.size();
	SseArchive::SystemLog() << "Act " << getId() << ": "
		<<  nDxsReportedIn
		<< " dx(s) reported in with '"
		<< expectedMessageDescrip << "' out of "
		<< nActUnitsStillWorking_
		<< " still working " << endl;

#if 1

	if (nDxsReportedIn > 0)
	{
		// debug -- print the names of the dxs that reported in
		stringstream reportedInNames;
		for (ActUnitList::const_iterator it = actUnitsReportedIn.begin();
				it != actUnitsReportedIn.end(); ++it)
		{
			ActivityUnit *actUnit = *it;
			reportedInNames << actUnit->getDxName() << " ";
		}
		SseArchive::SystemLog() << "Act " << getId() << ": "
			<< "These dxs reported in: "
			<< reportedInNames.str() << endl;
	}
#endif 

	// Figure out which actUnits/dxs did not report in:
	actUnitsStarted.sort(); 

	ActUnitList sortedActUnitsReportedIn(actUnitsReportedIn);
	sortedActUnitsReportedIn.sort();  

	ActUnitList actUnitsFailedToReport;  // the difference

	set_difference(actUnitsStarted.begin(), actUnitsStarted.end(),
			sortedActUnitsReportedIn.begin(), 
			sortedActUnitsReportedIn.end(),
			back_inserter(actUnitsFailedToReport));

	// log the names of the dxs that did not report in,
	// and take appropriate error handling action

	stringstream failedDxNames;
	for (ActUnitList::iterator it = actUnitsFailedToReport.begin();
			it != actUnitsFailedToReport.end(); ++it)
	{
		ActivityUnit *actUnit = *it;
		failedDxNames << actUnit->getDxName() << " ";

		actUnit->stop(dbParameters_);
		actUnit->shutdown();  // shutdown dx software
		actUnit->resetSocket();
	}
	SseArchive::SystemLog() 
		<< "Act " << getId() << ": "
		<< "Sent 'stop', 'shutdown' and reset socket on these dxs: "
		<< failedDxNames.str() << endl;

	SseArchive::ErrorLog()
		<< "Act " << getId() << ": "
		<< "Shutdown dxs that failed to report in with '" 
		<< expectedMessageDescrip
		<< "': " << failedDxNames.str() << endl;

	// If there's at least one remaining dx,
	// then continue on with the next step in the activity sequence.

	if (nDxsReportedIn > 0)
	{
		// continue with activity
		(this->*methodToCallOnSuccess)();
	}
	else
	{
		SseArchive::SystemLog() 
			<< "Act " << getId() << ": "
			<< "All dxs failed to report in with '"
			<< expectedMessageDescrip << "'"
			<< endl;

		// The actUnitComplete method should be able to
		// handle the rest of the error handling 
		// -- the shutdown/reset socket commands
		// should force an activity complete message to be issued
		// for each actUnit/dx, causing the activity to wrap up.

	}

}

// one or more of the dxs did not report in as
// tuned (aka activity unit ready).

void ObserveActivityImp::handleDxTunedTimeout()
{
	handleDxTimeout("dx tuned", actUnitReadyList_,
			& ObserveActivityImp::processTunedDxs);

}


// one or more of the dxs did not report data collection
// complete.  
void ObserveActivityImp::handleDataCollectionCompleteTimeout()
{
	handleDxTimeout("Data Collection complete", actUnitDataCollCompleteList_,
			& ObserveActivityImp::processAllDataCollectionComplete);
}

// one or more of the dxs did not report 'done sending cw coherent signals'
void ObserveActivityImp::handleDoneSendingCwCoherentSignalsTimeout()
{
	handleDxTimeout("done sending CW Coherent signals",
			actUnitDoneSendingCwCoherentSignalsList_,
			& ObserveActivityImp::processAllDoneSendingCwCoherentSignals);
}


// One or more of the dxs did not report in with 
// an activity complete message.  Since this the last
// stage of the dx processing for the activity unit,
// the 'methodToCallOnSuccess' is 'doNothing',
// ie, let handleDxTimeout just do error handling.

void ObserveActivityImp::handleActUnitCompleteTimeout()
{
	handleDxTimeout("act unit complete",
			actUnitCompleteList_,
			& ObserveActivityImp::doNothing);
}

void ObserveActivityImp::doNothing()
{
	// don't do anything.
	// this is here for convenience as a callback
}


string ObserveActivityImp::getTargetIdsForAllBeamsInUse()
{
	vector<string> allBeamsToUse(schedulerParameters_.getBeamsToUse());

	stringstream strm;

	for (vector<string>::iterator it = allBeamsToUse.begin();
			it != allBeamsToUse.end(); ++it)
	{
		string & beamName = *it;

		strm << beamName << ": " 
			<< actParameters_.getTargetIdForBeam(beamName) << " ";
	}

	TargetId targetId(-1);
	if (actParameters_.getPrimaryBeamPositionType() == 
			ActParameters::PRIMARY_BEAM_POS_TARGET_ID)
	{
		targetId = actParameters_.getPrimaryTargetId();
	}

	strm << "primary: " << targetId << " ";

	return strm.str();
}

/*
   Create and send recent RFI masks to dxs.  This must be
   thread safe for execution by the workTask thread.
   */
void ObserveActivityImp::sendRecentRfiMask()
{ 
	ActUnitList actUnitList(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp:  asking " <<
			actUnitList.size() << " activity units to send recent rfi mask"
			<< endl;);

	const string logActionText("sending recent RFI masks to DXs");

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Begin " << logActionText << "..." 
		<< endl;

	vector<TargetId> targetsToExclude;
	findTargetsInPrimaryBeamFov(dbParametersForWorkThread_.getDb(), 
			getMinDxSkyFreqMhz(),
			targetsToExclude);

	ActUnitList::iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it)
	{
		ActivityUnit *actUnit(*it);

		actUnit->sendRecentRfiMask(dbParametersForWorkThread_.getDb(),
				targetsToExclude);

	}

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Done " << logActionText << endl;

}


/* 
   Find all targets that fall within the field of view of the
   primary beam for the "nominal" target id.
   */
void ObserveActivityImp::findTargetsInPrimaryBeamFov(
		MYSQL *conn,
		double obsFreqMhz,
		vector<TargetId> & targetIdsInFov)
{
	// Look up coordinates of nominal target for this beam
	double ra2000Rads(primaryCenterRaRads_);
	double dec2000Rads(primaryCenterDecRads_);

	// Find the min & max ra/dec ranges covered by the primary
	// and look up all the targets that fall within that range.

	double halfBeamsizeRads(0.5 * 
			calculatePrimaryBeamsizeRads(obsFreqMhz));

	/* Expand beyond the nominal beam size
	   to get all targets that could still be detected
	   with reasonable sensitivity.
	   */
	double primaryBeamFirstNullPointFactor(2);
	halfBeamsizeRads *= primaryBeamFirstNullPointFactor;

	double minRaLimitHours(-1);
	double maxRaLimitHours(-1);
	double minDecLimitDeg(-1);			   
	double maxDecLimitDeg(-1);

	const double NorthPoleDecRads(0.5 * M_PI);
	bool crossesNorthPol(dec2000Rads + halfBeamsizeRads >= NorthPoleDecRads);
	if (crossesNorthPol)
	{
		/*
		   The beam crosses the north pole, so get targets at all RA's
		   that are north of the southern edge of the beam.  This of
		   course covers more area that just the beam, but it's a lot
		   simpler than trying to map out the ra/dec coordinates of the
		   edges of the beam at all RA's.  Any additional targets that
		   are picked up should have no adverse effect on the creation of the
		   recent rfi mask. 

TBD: add handling of south pole
*/

		minRaLimitHours = 0.0;

		const double HoursPerDay(24.0);
		maxRaLimitHours = HoursPerDay;

		minDecLimitDeg = SseAstro::radiansToDegrees(
				dec2000Rads - halfBeamsizeRads);			   

		maxDecLimitDeg = SseAstro::radiansToDegrees(NorthPoleDecRads);
	}
	else
	{
		// Add +- halfBeamsizeRads to the target position
		// to get the min & max ra/dec limits of a bounding box
		// containing the beam.

		double northRaRads;
		double northDecRads;
		OffPositions::moveNorth(ra2000Rads, dec2000Rads,
				halfBeamsizeRads,
				&northRaRads, &northDecRads);

		double southRaRads;
		double southDecRads;
		OffPositions::moveSouth(ra2000Rads, dec2000Rads,
				halfBeamsizeRads,
				&southRaRads, &southDecRads);

		double eastRaRads;
		double eastDecRads;
		OffPositions::moveEast(ra2000Rads, dec2000Rads,
				halfBeamsizeRads,
				&eastRaRads, &eastDecRads);

		double westRaRads;
		double westDecRads;
		OffPositions::moveWest(ra2000Rads, dec2000Rads,
				halfBeamsizeRads,
				&westRaRads, &westDecRads);

		minRaLimitHours = SseAstro::radiansToHours(westRaRads);
		maxRaLimitHours = SseAstro::radiansToHours(eastRaRads);
		minDecLimitDeg = SseAstro::radiansToDegrees(southDecRads);			   
		maxDecLimitDeg = SseAstro::radiansToDegrees(northDecRads);

	}

	findTargetsInRaDecRanges(conn, minRaLimitHours, maxRaLimitHours,
			minDecLimitDeg, maxDecLimitDeg,
			targetIdsInFov);


	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "ObserveActivityImp::findTargetsInPrimaryBeamFov "
			<< "obsFreqMhz: " << obsFreqMhz 
			<< ", using expanded search radius of " 
			<< halfBeamsizeRads << " rads, " 
			<< "found " << targetIdsInFov.size() << " targetIds"
			<< endl;);

	vector<string> allBeamsToUse(schedulerParameters_.getBeamsToUse());


#if 1
	// Verify that the nominal target ids are in the list.
	// For testing purposes, the target might
	// not be set up for autoscheduling and thus not show up
	// so this is just a warning.

	for (vector<string>::iterator it = allBeamsToUse.begin();
			it != allBeamsToUse.end(); ++it)
	{
		string & beamName = *it;
		TargetId targetId = actParameters_.getTargetIdForBeam(beamName);

		if (find(targetIdsInFov.begin(), targetIdsInFov.end(), targetId)
				== targetIdsInFov.end())
		{
			stringstream strm;
			strm << "Warning: target ID " << targetId 
				<< " not found when checking for targets in the"
				<< " primary beam FOV"
				<< endl;
			SseMessage::log(MsgSender, getId(),
					SSE_MSG_INFO, SEVERITY_WARNING,
					strm.str(), __FILE__, __LINE__);
		}
	}
#endif

	/* 
	   Add in all targets for the current observation.
	   This is mainly to take care of moving targets and any
	   others that are not flagged for automatic selection.
	   Anything else should have already been picked up in the query.
	   */

	for (vector<string>::iterator it = allBeamsToUse.begin();
			it != allBeamsToUse.end(); ++it)
	{
		string & beamName = *it;
		targetIdsInFov.push_back(actParameters_.getTargetIdForBeam(beamName));
	}

}

/*
   Find all the targetIds that fall within the ra & dec limits.
   Targets are added to the targetIdsInFov container.
   */
void ObserveActivityImp::findTargetsInRaDecRanges(
		MYSQL *conn,
		double minRaLimitHours, double maxRaLimitHours,
		double minDecLimitDeg, double maxDecLimitDeg,
		vector<TargetId> & targetIdsInFov)
{
#if 0
	cout << "findTargetsInRaDecRanges: "
		<< " minRaLimitHours: " << minRaLimitHours
		<< " maxRaLimitHours: " << maxRaLimitHours
		<< " minDecLimitDeg: "  << minDecLimitDeg
		<< " maxDecLimitDeg: "  << maxDecLimitDeg
		<< endl;
#endif

	stringstream sqlStmt;
	sqlStmt.precision(PrintPrecision);
	sqlStmt.setf(std::ios::fixed);  // show all decimal places up to precision

	sqlStmt << "select targetId,"
		<< " ra2000Hours, dec2000Deg"
		<< " from TargetCat where"
		<< " (dec2000Deg >= " << minDecLimitDeg
		<< " and dec2000Deg <= " << maxDecLimitDeg << ")";

	// handle RA wrap-around at zero
	string raConjunction("and");
	if (minRaLimitHours > maxRaLimitHours)
	{
		raConjunction = "or";
	}
	sqlStmt << " and"
		<< " (ra2000Hours >= " << minRaLimitHours  
		<< " " << raConjunction 
		<< " ra2000Hours <= " << maxRaLimitHours << ")";

	// only consider targets tagged for automatic selection
	// (ie, ignore all "non-active" target cat entries)
	sqlStmt << " and autoSchedule = 'Yes'";

	enum resultCols {targetIdCol, ra2000HoursCol, dec2000DegCol, numCols};

	MysqlQuery query(conn);
	query.execute(sqlStmt.str(), numCols, __FILE__, __LINE__);

#if 0
	cout << sqlStmt.str() << endl;
#endif

	while (MYSQL_ROW row = mysql_fetch_row(query.getResultSet()))
	{
		TargetId targetId(query.getInt(row, targetIdCol, 
					__FILE__, __LINE__));

#if 0
		double ra2000Hours(query.getDouble(row, ra2000HoursCol, 
					__FILE__, __LINE__));

		double dec2000Deg(query.getDouble(row, dec2000DegCol, 
					__FILE__, __LINE__));

		cout << " targetId: " << targetId
			<< " ra2000Hours: " << ra2000Hours
			<< " dec2000Deg: " << dec2000Deg
			<< endl;
#endif

		targetIdsInFov.push_back(targetId);
	}

#if 0
	cout << "nTargetIds: " << targetIdsInFov.size() << endl;
	cout << endl;
#endif   

}



/*
   This must be thread safe for execution by the workTask thread.
   */
void ObserveActivityImp::prepareFollowupCandidateSignals()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp::prepareFollowupCandidateSignals" << endl;);

	const string logActionText("preparing followup candidates");

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Begin " << logActionText << "..." 
		<< endl;

	ActUnitList actUnitList(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	ActUnitList::iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it)
	{
		ActivityUnit *actUnit(*it);

		// ask the activity units to look up the follow up signals
		// from the previous activity id

		actUnit->prepareFollowUpCandidateSignals(
				dbParametersForWorkThread_.getDb(),
				actParameters_.getPreviousActivityId());

	}

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Done " << logActionText << endl;

}

// returns total number of signals sent
int ObserveActivityImp::sendFollowupCandidateSignals(const NssDate &startTime)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp::sendFollowupCandidateSignals" << endl;);

	const string logActionText("sending followup candidates to DXs");

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Begin " << logActionText << "..." 
		<< endl;

	int totalFollowUpSignalCount = 0;

	ActUnitList actUnitList(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	ActUnitList::iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it)
	{
		ActivityUnit *actUnit(*it);

		// ask the activity units to look up the follow up signals
		// from the previous activity id and forward them to the dx

		totalFollowUpSignalCount += 
			actUnit->sendFollowUpCandidateSignals(startTime);
	}

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Done " << logActionText << ","
		<< " total count: " << totalFollowUpSignalCount
		<< endl;

	return totalFollowUpSignalCount;
}

void ObserveActivityImp::sendStartTime(const NssDate & startTime)
{
	StartActivity startAct;
	startAct.startTime = startTime;

	ActUnitList actUnitList(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: sending start time to " <<
			actUnitList.size() << " activity units" << endl;);

	ActUnitList::iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it)
	{
		ActivityUnit *actUnit(*it);
		actUnit->setStartTime(startAct); 
	}
}

void ObserveActivityImp::
startActUnitWatchdogTimers()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp::startActUnitWatchdogTimers" << endl;);

	// Start a watchdog timer, waiting for all activity units to
	// report in as data collection complete

	int allowedReportingDelaySecs = 
		actParameters_.getDataCollCompleteTimeoutOffsetSecs();
	int baselineAccumulationTimeSecs =  static_cast<int> (
                        dxParameters_.getDxActParamStruct().baselineInitAccumHalfFrames *
                        DxHalfFrameLengthSecs);

	int dcWaitDurationSecs = startTimeOffset_ + dataCollectionLengthSecs_ +
		baselineAccumulationTimeSecs + allowedReportingDelaySecs;

	startWatchdogTimer(dataCollectionCompleteTimeout_,
			&ObserveActivityImp::handleDataCollectionCompleteTimeout,
			dcWaitDurationSecs);

	int estimatedSignalDetectionLength = actParameters_.getSigDetWaitFactor()
		* dataCollectionLengthSecs_;

	// start a watchdog timer, waiting for all doneSendingCwCoherentSignals.
	// The wait time is a fraction of the total time allowed for signal detection.

	double doneSendingCwCoherentSignalsWaitFraction = 
		actParameters_.getDoneSendingCwCohSigsTimeoutFactorPercent();
	int doneSendingCwCoherentSignalsWaitDurationSecs = 
		static_cast<int>(startTimeOffset_ + dataCollectionLengthSecs_ +
				 baselineAccumulationTimeSecs +
				(estimatedSignalDetectionLength * doneSendingCwCoherentSignalsWaitFraction));

	startWatchdogTimer(doneSendingCwCoherentSignalsTimeout_,
			& ObserveActivityImp::handleDoneSendingCwCoherentSignalsTimeout,
			doneSendingCwCoherentSignalsWaitDurationSecs);

	// Start a watchdog timer, waiting for all activity units to
	// report in as activity complete.

	int sigDetectWaitDurationSecs = startTimeOffset_ + dataCollectionLengthSecs_ +
	                baselineAccumulationTimeSecs + estimatedSignalDetectionLength;

	startWatchdogTimer(actUnitCompleteTimeout_,
			&ObserveActivityImp::handleActUnitCompleteTimeout,
			sigDetectWaitDurationSecs);
}

void ObserveActivityImp::
dataCollectionStarted(ActivityUnit *activityUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: dataCollectionStarted " << activityUnit << endl;);

	nActUnitsDataCollectionStarted_++;

	if (nActUnitsDataCollectionStarted_ == nActUnitsStillWorking_)
	{

		updateStatus(ObserveActivityStatus::DATA_COLLECTION_STARTED);

		obsSummaryTxtStrm_ << "Data Collection Started:   "
			<< SseUtil::currentIsoDateTime() << endl;

		// give feedback to text UI.  TBD -- move elsewhere?
		cout << "Activity " << getId() << " data collection started" << endl;

	}
}


void ObserveActivityImp::processAllDataCollectionComplete()
{
	if (! processAllDataCollCompleteAlreadyRun_)
	{
		processAllDataCollCompleteAlreadyRun_ = true;

		updateStatus(ObserveActivityStatus::DATA_COLLECTION_COMPLETE);

		obsSummaryTxtStrm_  << "Data Collection Complete:  "
			<< SseUtil::currentIsoDateTime() << endl;

		allDataCollectionCompleteWrapup();

		// notify the scheduler
		getActivityStrategy()->dataCollectionComplete(this);

	}
}


void ObserveActivityImp::
dataCollectionComplete(ActivityUnit *actUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: dataCollectionComplete " << actUnit << endl;);

	nActUnitsDataCollectionComplete_++;

	// keep track of which ones reported in.
	actUnitDataCollCompleteList_.push_back(actUnit);   

	if (nActUnitsDataCollectionComplete_ == nActUnitsStillWorking_)
	{
		dataCollectionCompleteTimeout_.cancelTimer();

		SseArchive::SystemLog()  
			<< "Act " << getId() << ": "
			<< nActUnitsReady_ 
			<< " Activity Unit(s) (dxs) report Data Coll complete " << endl;

		processAllDataCollectionComplete();
	}
}

void ObserveActivityImp::
signalDetectionStarted (ActivityUnit* activityUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: signalDetectionStarted" << endl;);


	nActUnitsSignalDetectionStarted_++;

	if (nActUnitsSignalDetectionStarted_ == nActUnitsStillWorking_)
	{
		updateStatus(ObserveActivityStatus::SIGNAL_DETECTION_STARTED);

		obsSummaryTxtStrm_ << "Signal Detection Started:  "
			<< SseUtil::currentIsoDateTime() << endl;

		// give feedback to text UI.  TBD -- move elsewhere?
		cout << "Activity " << getId() << " signal detection started" << endl;

	}
}

void ObserveActivityImp::
signalDetectionComplete (ActivityUnit* activityUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: signalDetectionComplete" << endl;);

	nActUnitsSignalDetectionComplete_++;

	if (nActUnitsSignalDetectionComplete_ == nActUnitsStillWorking_)
	{
		updateStatus(ObserveActivityStatus::SIGNAL_DETECTION_COMPLETE);

		obsSummaryTxtStrm_ << "Signal Detection Complete: "
			<< SseUtil::currentIsoDateTime() << endl;
	}
}

void ObserveActivityImp::processAllDoneSendingCwCoherentSignals()
{
	if (! processAllDoneSendingCwCoherentSignalsAlreadyRun_)
	{
		processAllDoneSendingCwCoherentSignalsAlreadyRun_ = true;

		if (isMultitargetObservation() || forceArchivingAroundCenterTuning())
		{ 
			PutCmdPatternInWorkTaskQueue(
					SendCandidatesToDxsForSecondaryProcessingCmd);
		}
	}
}

void ObserveActivityImp::
doneSendingCwCoherentSignals(ActivityUnit* actUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "ObserveActivityImp: doneSendingCwCoherentSignals"
			<< " dx: " << actUnit->getDxName() 
			<< " nActUnitsStillWorking_: " << nActUnitsStillWorking_
			<< endl;);

	nActUnitsDoneSendingCwCoherentSignals_++;

	// keep track of which ones reported in.
	actUnitDoneSendingCwCoherentSignalsList_.push_back(actUnit);   

	if (nActUnitsDoneSendingCwCoherentSignals_ == nActUnitsStillWorking_)
	{
		doneSendingCwCoherentSignalsTimeout_.cancelTimer();

		obsSummaryTxtStrm_ << "doneSendingCwCoherentSignals for all dxs: "
			<< SseUtil::currentIsoDateTime() << endl;

		processAllDoneSendingCwCoherentSignals();
	}
}

// Done with secondary processing
void ObserveActivityImp::
doneSendingCandidateResults(ActivityUnit* activityUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp::doneSendingCandidateResults" << endl;);

	if (! isMultitargetObservation() && !forceArchivingAroundCenterTuning())
	{
		stringstream strm;
		strm << "received doneSendingCandidateResults message from dx "
			<< activityUnit->getDxName() 
			<< " but appropriate mode is not enabled\n";

		SseMessage::log(MsgSender, getId(),
				SSE_MSG_INVALID_MSG, SEVERITY_WARNING, 
				strm.str(), __FILE__, __LINE__);

		return;
	}

	nActUnitsDoneSendingCandidateResults_++;

	if (nActUnitsDoneSendingCandidateResults_ == nActUnitsStillWorking_)
	{
		obsSummaryTxtStrm_ << "doneSendingCandiateResults for all dxs: "
			<< SseUtil::currentIsoDateTime() << endl;

		PutCmdPatternInWorkTaskQueue(ResolveCandidatesCmd);
	}
}

/*
   This must be thread safe for execution by the workTask thread.
   Uses db connection reserved for methods invoked by the work task 
   thread to avoid thread conflict.
   */
void ObserveActivityImp::sendCandidatesToDxsForSecondaryProcessing()
{
	const string logActionText(
			"sending candidates to DXs for secondary processing");

	SseArchive::SystemLog()
		<< "Act " << getId() << ": "
		<< "Begin " << logActionText << "..." << endl;

	ActUnitList actUnitList(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	ActUnitList::iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it) 
	{
		ActivityUnit *actUnit = *it;

		actUnit->sendCandidatesForSecondaryProcessing(
				dbParametersForWorkThread_.getDb());
	}

	SseArchive::SystemLog()
		<< "Act " << getId() << ": "
		<< "Done " << logActionText << endl;

}


void ObserveActivityImp::resolveCandidatesBasedOnSecondaryProcessingResults()
{
	SseArchive::SystemLog()
		<< "Act " << getId() << ": "
		<< "Begin resolving candidates based on secondary processing results..." 
		<< endl;

	ActUnitList actUnitList(
			actUnitStillWorkingListMutexWrapper_.getListCopy());

	ActUnitList::iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it) 
	{
		ActivityUnit *actUnit = *it;
		actUnit->resolveCandidatesBasedOnSecondaryProcessingResults(
				dbParametersForWorkThread_.getDb());
	}

	SseArchive::SystemLog()
		<< "Act " << getId() << ": "
		<< "Done resolving candidates based on secondary processing results" 
		<< endl;

}



void ObserveActivityImp::activityUnitObsSummary(ActivityUnit *activityUnit,
		ObsSummaryStats &obsSumStats)
{
	combinedObsSummaryStats_ += obsSumStats;
}



void ObserveActivityImp::activityUnitFailed(ActivityUnit *activityUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "ObserveActivityImp: activityUnitFailed" 
			<< " dx: " << activityUnit->getDxName() << endl;);

	nActUnitsFailed_++;
	nActUnitsStillWorking_--;

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "ObserveActivityImp: activityUnitFailed" 
			<< " nActUnitsFailed_: " << nActUnitsFailed_
			<< " nActUnitsStillWorking_: " << nActUnitsStillWorking_
			<< endl;);

	Assert(nActUnitsStillWorking_ >= 0);

	stringstream errorMsgStrm; 
	errorMsgStrm << "Activity Unit " << activityUnit->getId()
		<< " *Failed*."
		<< "  Dxs: " << activityUnit->getDxName()
		<< endl;

	// Activity units currently log their own failures so keep this
	// commented out until that changes.
	//    SseArchive::SystemLog() << "Act " << getId() << ": " << errorMsgStrm.str();

	obsSummaryTxtStrm_ << errorMsgStrm.str();

	activityUnitComplete(activityUnit);

}

void ObserveActivityImp::activityUnitComplete(ActivityUnit *activityUnit)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "ObserveActivityImp::activityUnitComplete" 
			<< "dx: " << activityUnit->getDxName() << endl;);

	// keep track of which ones reported in.
	actUnitCompleteList_.push_back(activityUnit);   

	actUnitStillWorkingListMutexWrapper_.removeFromList(activityUnit);

	// once all actUnits report in complete
	// tell schedulerStrategy activity is complete
	// (these counters are just a crude prototype, need better
	// accounting later).
	nActUnitsDone_++;
	nActUnitsStillWorking_--;

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			"ObserveActivityImp::activityUnitComplete() "
			<< " nActUnitsDone=" << nActUnitsDone_ 
			<< " nActUnitsStarted=" << nActUnitsStarted_ << endl;);

	if (nActUnitsDone_ == nActUnitsStarted_)
	{
		VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
				<< "ObserveActivityImp::activityUnitComplete() "
				<< " all act units accounted for." << endl;)

			// shouldn't be any actunits left on the working list
			Assert(actUnitStillWorkingListMutexWrapper_.listSize() == 0);

		// cancel the 'wait for dx activity complete messages' watchdog
		actUnitCompleteTimeout_.cancelTimer();

		if (!stopReceived_.get())
		{
			if (nActUnitsFailed_ == nActUnitsStarted_)
			{
				terminateActivity("all activity units (dxs) failed");
				return;
			} 

			if (nActUnitsFailed_ > 0)
			{
				SseArchive::SystemLog() << "Act " << getId() << ": "
					<< nActUnitsFailed_ 
					<< " Activity Unit(s) failed out of "
					<< nActUnitsStarted_ << " started."
					<< endl;

				obsSummaryTxtStrm_ << nActUnitsFailed_ 
					<< " Activity Unit(s) failed." 
					<< endl;
			} 

			int nSuccessfulActUnits = nActUnitsDone_ - nActUnitsFailed_;

			SseArchive::SystemLog() << "Act " << getId() << ": "
				<< nSuccessfulActUnits
				<< " Activity Unit(s) completed successfully."
				<< endl;

			obsSummaryTxtStrm_ << nSuccessfulActUnits
				<< " Activity Unit(s) completed successfully."
				<< endl;

			activityComplete();
		}

	}

}

// Compare the data product outputs of all the dxs
// used in this activity.  
void ObserveActivityImp::compareDxDataResults()
{
	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Running dx data product comparison:" << endl;

	// make sure there were at least 2 activity units in this activity
	if (nActUnitsStarted_ < 2)
	{
		SseArchive::SystemLog() << "Act " << getId() << ": "
			<< "Can't run comparison since only 1 Dx"
			<< " (or Dx Pair) was used." << endl;
		return;
	}

	string outfile = getArchiveFilenamePrefix() + ".dxcompare-results.txt";

	stringstream cmd;
	cmd << "compareDxData"
		<< " -act " << getId()
		<< " -dir " << getDataProductsDir()
		<< " -verbose "
		<< " > " << outfile;

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " <<
			"Running command: " << cmd.str() << endl);

	stringstream results;
	results << "Act " << getId() << ": Dx comparison test results: ";

	int status = system(cmd.str().c_str());
	if (status == 0)
	{
		results << "SUCCESS: All data products matched. \n"
			<< "Detailed results stored in " << outfile << endl;
	}
	else
	{
		results << "**FAILURE: All data products did NOT match**. \n" 
			<< "Detailed results stored in " << outfile << endl;
	} 

	SseArchive::SystemLog() << results.str();
	obsSummaryTxtStrm_ << results.str();

}


void ObserveActivityImp::cancelPendingTimers()
{
	// Cancel any pending timers (except for delete timer)
	// This is harmless if timer is not active.

	tscopeReadyTimeout_.cancelTimer();
	ifcReadyTimeout_.cancelTimer();
	tsigReadyTimeout_.cancelTimer();
	dxTunedTimeout_.cancelTimer();
	dataCollectionCompleteTimeout_.cancelTimer();
	doneSendingCwCoherentSignalsTimeout_.cancelTimer();
	actUnitCompleteTimeout_.cancelTimer();
	stopTimeout_.cancelTimer();
	stopActUnitsTimeout_.cancelTimer();
	completeDxPrepNotification_.cancelTimer();
	pointAntsAndWaitTimeout_.cancelTimer();

	// don't include actDeleteTimeout_ 
}

/* 
   Assumes that this method will only be called
   by the scheduler thread (via the start() method call chain)
   or the reactor thread, but not the work task thread,
   so that a safe mysql connection can be used.
   */
void ObserveActivityImp::terminateActivity(const string &errorMsg)
{
	// prevent this method from being called multiple times,
	// or from being called while activityComplete is being executed.

	if (!activityWrappingUp_.get()) 
	{
		activityWrappingUp_.set(true);

		VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
				"terminateActivity called: " << errorMsg <<  endl); 

		cancelPendingTimers();

		PutThreadExitMessageInWorkTaskQueue;

		stopActivityUnits();

		if (useTscope())
		{
			TscopeList &tscopeList = getAllNeededTscopes();
			for (TscopeList::iterator tscopeProxy = tscopeList.begin(); 
					tscopeProxy != tscopeList.end(); tscopeProxy++)
			{
				(*tscopeProxy)->beamformerStop();
			}
		}

		detachAllNonDxComponents();  

		stringstream strm;
		strm  << "Activity failed: " << errorMsg << endl;

		SseMessage::log(MsgSender, getId(),
				SSE_MSG_ACT_FAILED, SEVERITY_ERROR, 
				strm.str(), __FILE__, __LINE__);

		obsSummaryTxtStrm_ << "Activity terminated due to error: " 
			<< errorMsg << endl;
		if (dbParametersForTerminate_.useDb()) 
		{
			updateDbErrorComment(dbParametersForTerminate_, errorMsg);
		}

		activityFailed();

	}

}


void ObserveActivityImp::activityFailed()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp::activityFailed" << endl;);

	updateStatus(ObserveActivityStatus::ACTIVITY_FAILED);

	bool failed = true;
	getActivityStrategy()->activityComplete(this, failed);

	// Don't do anything after this point.
	// Assume this object will be deleted immediately
	// after reporting in as complete.

}


void ObserveActivityImp::logObsSummaryStats()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp::logObsSummaryStats" << endl;);

	obsSummaryTxtStrm_ << endl
		<< combinedObsSummaryStats_ << endl
		<< "Activity Complete: "
		<< SseUtil::currentIsoDateTime()
		<< endl;

	SseArchive::SystemLog() << "Act " << getId() << ":" << endl
		<<	combinedObsSummaryStats_ << endl; 

	int totalConfirmedCandidates = combinedObsSummaryStats_.confirmedCwCandidates
		+ combinedObsSummaryStats_.confirmedPulseCandidates;

	//    if (totalConfirmedCandidates > 0)
	{
		SseArchive::SystemLog() 
			<< "Act " << getId() << ":" 
			<< " " << totalConfirmedCandidates 
			<< " total confirmed candidates "
			<< "(" 
			<< combinedObsSummaryStats_.confirmedCwCandidates << " cw + "
			<< combinedObsSummaryStats_.confirmedPulseCandidates << " pulse"
			<< ")"
			<< endl;


		// print act summary in format that assists paper log entry
		stringstream strm;
		//strm.precision(1);           // show N places after the decimal
		//strm.setf(std::ios::fixed);  // show all decimal places up to precision

		strm << "Summary:" << endl
			<< "Act: " << getId() 
			<< "  TargetIds: " << getTargetIdsForAllBeamsInUse()
			<< "  Min/Max: " << getMinDxSkyFreqMhz() << "/" << getMaxDxSkyFreqMhz() 
			<< "  Type: " << getActivityType() 
			<< "  #Conf: " << totalConfirmedCandidates 
			<< " (" 
			<< combinedObsSummaryStats_.confirmedCwCandidates << " cw + "
			<< combinedObsSummaryStats_.confirmedPulseCandidates << " pul"
			<< ")"
			<< "" <<  endl;

		SseArchive::SystemLog() << strm.str();
	}

	// debug 
	//combinedObsSummaryStats_.printInExpandedFormat(obsSummaryTxtStrm_);

}

void ObserveActivityImp::finishNonDxActivity()
{
	storeParametersInDb();

	detachAllNonDxComponents();

	activityComplete();
}

void ObserveActivityImp::activityComplete()
{
	// Indicate that we're wrapping up (so that a stop command cannot
	// go off while or after the activity is wrapping up).
	activityWrappingUp_.set(true);

	cancelPendingTimers();

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"beginning ObserveActivityImp: activityComplete" << endl;);

	PutThreadExitMessageInWorkTaskQueue;

	if (useDx())
	{
		logObsSummaryStats();

		if (actParameters_.compareDxDataProducts())
		{
			compareDxDataResults();
		}
	}

	if (dbParameters_.useDb())
	{
		updateActivityStatistics();
	}

	// Notify the scheduler if there are any confirmed candidates
	// to follow up
	if (schedulerParameters_.followupEnabled() && 
			! doNotReportConfirmedCandidatesToScheduler())
	{
		if (combinedObsSummaryStats_.confirmedCwCandidates > 0 ||
				combinedObsSummaryStats_.confirmedPulseCandidates > 0)
		{
			getActivityStrategy()->foundConfirmedCandidates(this);
		}
	}

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"ObserveActivityImp: activityComplete" << endl;);

	updateStatus(ObserveActivityStatus::ACTIVITY_COMPLETE);

	// close the observation summary file
	obsSummaryTxtStrm_.close();

	bool failed = false;
	getActivityStrategy()->activityComplete(this, failed);

	// Don't do anything after this point.
	// Assume this object will be deleted immediately
	// after reporting in as complete.

}

// ----- utilities ------

// Calculate the absolute time in secs for the start of an observation.
int ObserveActivityImp::calculateStartTime()
{
	// current time, in abs seconds since the epoch
	time_t currentTime = time(NULL);   

	// when to start the obs, in abs seconds
	time_t startTimeSecs = currentTime + startTimeOffset_;

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "data collection scheduled start time: " 
			<< SseUtil::isoDateTime(startTimeSecs) << endl);

	// log start time
	stringstream strm;

	strm << "Scheduled DX Start Time: "
		<< SseUtil::isoDateTime(startTimeSecs) 
		<< " (" << startTimeSecs << " secs)"
		<< endl;

	obsSummaryTxtStrm_ << strm.str();

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< strm.str();

	return startTimeSecs;
}


// calculate the setup time (time offset) in secs needed to prepare
// for the start of an observation.

int ObserveActivityImp::calculateStartTimeOffset()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "calculateStartTimeOffset()" << endl;);

	// calculate how much time the initial baseline accumulation will take

	int baselineInitAccumTimeSecs = static_cast<int> (
			dxParameters_.getDxActParamStruct().baselineInitAccumHalfFrames *
			DxHalfFrameLengthSecs);

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"baselineInitAccumTimeSecs: " << baselineInitAccumTimeSecs << endl;);

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"requested start delay in secs: " << actParameters_.getStartDelaySecs() << endl;);

         // Baseline Accumulation starts at the Scheduled DX Start time
	int startTimeOffset = actParameters_.getStartDelaySecs();

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"startTimeOffset in secs: " << startTimeOffset << endl;);

	return startTimeOffset;

}


void ObserveActivityImp::createObsSummaryFileStream()
{
	string obsSummaryFilename(getArchiveFilenamePrefix() + 
			".activity.summary.txt");

	SseUtil::openOutputFileStream(obsSummaryTxtStrm_, obsSummaryFilename);

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"opening file: " <<  obsSummaryFilename << endl); 
}



void ObserveActivityImp::writeObsSummaryHeader(
		const NssParameters &nssParameters)
{
	ostream &strm = obsSummaryTxtStrm_;

	strm << "==============================" << endl;
	strm << "Activity (Observation) Summary" << endl;
	strm << "==============================" << endl;
	strm << endl;

	strm << "Activity Type: " << getActivityType() << endl;
	strm << "Activity Id: " << getId() << endl;
	strm << "Activity Creation Time: ";
	strm << SseUtil::currentIsoDateTime() << endl;

	if (observingTargets()) 
	{
		strm << "TargetIds: " << getTargetIdsForAllBeamsInUse() << endl;
	}
	strm << SSE_VERSION << endl;
	strm << endl;    

	strm << nssParameters;
	strm << endl;
}

void ObserveActivityImp::updateStatus(ObserveActivityStatus::StatusEnum statusEnum)
{
	statusStage_.set(statusEnum); 
	updateActivityLog();
}

void ObserveActivityImp::updateActivityLog()
{
	// Make a system log entry
	// TBD merge or move this code 

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< getActivityType() << " " 
		<< statusStage_.getString() << endl;

	{
		// update the activity log
		// this is shared across activities so needs to be mutex protected

		ACE_Guard<ACE_Recursive_Thread_Mutex> guard(activityLogStreamMutex);

		ostream &strm(getActivityLogStream());

		strm.precision(1);           // show N places after the decimal
		strm.setf(std::ios::fixed);  // show all decimal places up to precision

		strm << SseUtil::currentIsoDateTime();

		strm << "  " << getActivityType(); 

		if (observingTargets()) 
		{
			strm << "  TargetIds: " << getTargetIdsForAllBeamsInUse();
		}

		strm << "  Dx Min/Max: " << getMinDxSkyFreqMhz()
			<< "/" << getMaxDxSkyFreqMhz() << " MHz";
		strm << "  Len: " << dataCollectionLengthSecs_ << "s";

		// print start time (if set)
		if (startTime_ > 0)
		{
			//strm << "  Start: " << SseUtil::isoDateTime(startTime_);
		}

		strm << "  Act " << getId();  // ActivityId
		strm << ": " << statusStage_.getString();
		strm << endl;
	}

}

// On first call, opens a stream to the activityLog file,
// which is shared across activities (and therefore threads).
// (The stream is static).
// On subsequent calls, returns the opened stream.
// Callers should use the activityLogStreamMutex to protect the stream.

static ofstream & getActivityLogStream()
{
	static ofstream activityLogStream; 

	if (! activityLogStream.is_open())
	{
		// try to open the log stream

		string logFilename = SseArchive::getArchiveTemplogsDir() + 
			"/sse-activity-log.txt";

		// open & truncate the file.
		activityLogStream.open(logFilename.c_str(), (ios::out | ios::trunc));
		if (!activityLogStream.is_open()) {
			cerr << "File Open failed on " << logFilename << endl;

			// TBD error handling.  
		}

	}   

	return activityLogStream;

}


void ObserveActivityImp::deleteAllActivityUnits() 
{
	ActUnitList::iterator it;
	for (it = actUnitCreatedList_.begin(); it != actUnitCreatedList_.end();
			++it)
	{
		ActivityUnit *actUnit = *it;	
		delete actUnit;
	}

}

#if 0
static bool substringFoundInMessage(const string &messageText, 
		const string &searchText)
{
	if (SseUtil::strToLower(messageText).find(
				SseUtil::strToLower(searchText)) != std::string::npos)
	{
		return true;
	}

	return false;
}
#endif

void ObserveActivityImp::processNssMessage(NssMessage &nssMessage)
{
	// only terminate on ERROR & FATAL messages

	if (nssMessage.severity == SEVERITY_ERROR ||
			nssMessage.severity == SEVERITY_FATAL)
	{
		terminateActivity(nssMessage.description);
		return;
	}

	// proxies log the message themselves so no need to do
	// it here
}


void ObserveActivityImp::ifcError(IfcProxy * ifcProxy, NssMessage &nssMessage)
{
	processNssMessage(nssMessage);
}

void ObserveActivityImp::tscopeError(TscopeProxy * tscopeProxy, 
		NssMessage &nssMessage)
{
	/*
	   Try to handle the case where ATA backend server
	   gets stuck reporting bad status.
	   Reconnecting may do the trick.
	   */
#if 0
	const string parseErrorText("error parsing");
	if (substringFoundInMessage(nssMessage.description, parseErrorText))
	{
		SseArchive::ErrorLog() << "ObserveActivityImp::tscopeError()"
			<< " error parsing tscope status, "
			<< " reconnecting to tscope ant server." 
			<< endl;
	}
#endif

#if 0
	string logMsg("ObserveActivityImp::tscopeError(): attempting to recover by disconnect & reconnect to tscope ant server.");

	SseArchive::ErrorLog() << logMsg << endl;
	SseArchive::SystemLog() << logMsg << endl;


	// Try reconnect in all error cases
	tscopeProxy->disconnect();
	sleep(1);
	tscopeProxy->connect();
#endif

	processNssMessage(nssMessage);
}

void ObserveActivityImp::testSigError(TestSigProxy * testSigProxy,
		NssMessage &nssMessage)
{
	processNssMessage(nssMessage);
}

//------------------------------------------------


// Start components

void ObserveActivityImp::startComponents()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"startComponents()" << endl;);

	// Determine where to start in the chain of actions, 
	// based on how the activity operations bit settings.

	// Set the if source first, before the other components
	// are used.  This method does not wait for a response from
	// the tsig.
	// if (useTestgen())
	{
		setIfSource();
	}

	if (useTscope())
	{
		startTelescopes();
	}
	else if (useIfc())
	{
		startIfcs(); 
	}
	else if (useTestgen())
	{
		startTestSigGens(); 
	}
	else  if(useDx()) //JR  - Added this if statement onto the else
	{	               //previously this was just an ELSE.
		SseArchive::SystemLog() << "***CALL startChan() " << endl;
		// Move start Channelizers here.
		SseArchive::SystemLog() << "***CALL startDxs() " << endl;
		startDxs();
	}
	else
	{
		SseArchive::SystemLog() << "No components to set up " << endl;
	}

}

void ObserveActivityImp::logError(const string &msg)
{
	stringstream strm;

	strm << "ObserveActivityImp::logError Act " << getId() << ": " << msg << endl;

	SseArchive::ErrorLog() << strm.str();
	SseArchive::SystemLog() << strm.str();
	cerr << strm.str();
}


void ObserveActivityImp::lookUpTargetCoordinates(
		MYSQL *conn, TargetId targetId,
		double *ra2000Rads, double *dec2000Rads,
		bool *moving)
{
	if (dbParameters_.useDb())
	{
		getTargetInfo(conn, targetId, ra2000Rads, dec2000Rads, moving);

		// If it's a moving target, then calculate its current position
		// (J2000 RA/Dec in radians)
		if (*moving)
		{
			calculateMovingTargetPosition(conn, targetId, ra2000Rads, dec2000Rads);
		}
	}
	else
	{
		// use some dummy target values for testing without the database

		stringstream strm;
		strm << "Database is turned off, can't look up target coords."
			<< " Using dummy (0,0) coords." << endl;
		SseMessage::log(MsgSender, getId(),
				SSE_MSG_DBERR, SEVERITY_WARNING,
				strm.str(), __FILE__, __LINE__);

		*moving = false;
		*ra2000Rads = 0.0;
		*dec2000Rads = 0.0;
	}


}

/*
   Calculate the J2000 coordinates of the moving target
   for the current time.
   Coordinates are returned  as J2000 RA/Dec in radians.
Inputs:
earth ephemeris file
target ephemeris file
dut (diff UTC & UT1)
Throws: SseException on failure
*/   

void ObserveActivityImp::calculateMovingTargetPosition(
		MYSQL *conn, TargetId targetId,
		double *ra2000Rads, double *dec2000Rads)
{
	string ephemFilename(getSpacecraftEphemFilename(conn, targetId));
	if (ephemFilename == "")
	{
		stringstream errorMsg;
		errorMsg << "Spacecraft ephemeris filename in database is blank for "
			<< "target " << targetId << endl;
		throw SseException(errorMsg.str(), __FILE__, __LINE__, 
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	SseArchive::SystemLog() << "Act " << getId() << ":"
		<< " Using ephemeris for moving target: "
		<< ephemFilename << endl;
	// use the current time
	time_t obsDate;
	time(&obsDate);

	// TBD is this still needed?
	//double diff_utc_ut1 = getActParameters().getDiffUtcUt1();    // DUT

	double rangeKm;
	double rangeRateKmSec;

	Spacecraft::calculatePosition(
			SseSystem::getSetupDir(),
			ephemFilename,
			getActParameters().getEarthEphemFilename(),
			obsDate,
			ra2000Rads,
			dec2000Rads,
			&rangeKm, &rangeRateKmSec
			);
}


void ObserveActivityImp::logPosition(const string &description, 
		double ra2000Rads, 
		double dec2000Rads)
{
	const int nominalPrecision(7);
	const int secsPrecision(3);
	int degHourMinWidth(2);
	int secWidth(degHourMinWidth+1+secsPrecision);  // +1 for decimal

	stringstream strm;
	strm.precision(nominalPrecision); // show N places after the decimal
	strm.setf(std::ios::fixed);  // show all decimal places up to precision

	strm << setfill('0');
	strm << description << " position (J2000): " << endl;

	// ---- 
	strm << "RA: ";

	// ra: decimal hours
	double raDecimalHours(SseAstro::radiansToHours(ra2000Rads));
	strm << raDecimalHours << " hours // ";

	// ra: hh mm ss
	int raHours;
	int raMin;
	double raSec;
	SseAstro::decimalHoursToHms(raDecimalHours, &raHours, &raMin, &raSec);

	strm << setw(degHourMinWidth) << raHours << " " 
		<< setw(degHourMinWidth) << raMin << " ";
	strm.precision(secsPrecision);
	strm << setw(secWidth) <<  raSec << " hms // ";

	// ra: decimal degrees
	strm.precision(nominalPrecision);

	strm << SseAstro::radiansToDegrees(ra2000Rads) << " deg"
		<< endl;

	// ---- 
	strm << "Dec: ";

	// dec: decimal degrees
	double decDecimalDegrees(SseAstro::radiansToDegrees(dec2000Rads));
	strm << decDecimalDegrees << " deg // ";

	// dec: deg min sec
	char sign;
	int decDeg;
	int decMin;
	double decSec;
	SseAstro::decimalDegreesToDms(decDecimalDegrees,
			&sign, &decDeg, &decMin, &decSec);

	strm << sign << setw(degHourMinWidth) << decDeg << " " 
		<< setw(degHourMinWidth) << decMin << " ";
	strm.precision(secsPrecision);
	strm << setw(secWidth) << decSec << " dms" << endl;

	// log it
	SseArchive::SystemLog() << "Act " << getId() << ": " << strm.str();
	obsSummaryTxtStrm_ << strm.str() << endl;

}


// calculate the estimated time between the start of the last
// activity (prevActStartTime) and the start of the next one (used for OFFs)

int ObserveActivityImp::calculateDeltaTimeBetweenObsSecs(
		int prevActStartTime)
{
	// Note: we don't have the new start time yet, since
	// that's calculated after the telescope is
	// tracking.  Use an estimate of the start time instead.

	int estimatedSlewTimeSecs = 0;  // TBD fill this in
	int estimatedReobsStartTime = SseMsg::currentNssDate().tv_sec +
		estimatedSlewTimeSecs + startTimeOffset_;

	int deltaTimeSecs = estimatedReobsStartTime - prevActStartTime;

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " 
			<< "calculateDeltaTimeSecsBetweenObs: " 
			<< " prevActStartTime=" << prevActStartTime
			<< " estimatedReobsStartTime=" << estimatedReobsStartTime
			<< " deltaTimeSecs=" << static_cast<int>(deltaTimeSecs)
			<< endl;);


	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Time since previous obs (plus offset): "
			<< deltaTimeSecs << " secs" << endl;

	AssertMsg(deltaTimeSecs > 0, "delta time since last obs is negative");

	const int secsPerHour = 3600;
	const int deltaTimeWarningPeriod = secsPerHour;
	if (deltaTimeSecs >  deltaTimeWarningPeriod)
	{
		SseArchive::SystemLog() << "Act " << getId() << ": "
			<< "Warning: it's been more than "
			<< deltaTimeWarningPeriod
			<< " secs since the previous observation" 
			<< endl;

	}

	return deltaTimeSecs;

}

// Look up the previous activity id in the database,
// and find the targetId for each beam, and start time (unix timestamp)
// of that activity.  Target Ids are stored in the actParameters,
// start time is stored in prevActStartTime_.
// Throws SseException on database errors.

void ObserveActivityImp::getPreviousActivityInfoFromDb(ActivityId_t prevActivityId)
{
	string methodName("getPreviousActivityInfoFromDb()");

	stringstream sqlstmt;
	stringstream errorMsg;

	sqlstmt << "SELECT targetbeam1, targetbeam2, targetbeam3,"
		<< "targetbeam4, targetbeam5, targetbeam6, "
		<< "UNIX_TIMESTAMP(Activities.startOfDataCollection), "
		<< "FROM_UNIXTIME(0) "
		<< "FROM ActParameters, Activities "
		<< "WHERE Activities.id = " << prevActivityId 
		<< " and Activities.actParametersId = ActParameters.id "
		<< " ";

	enum resultCols { targetbeam1Col, targetbeam2Col, targetbeam3Col,
		targetbeam4Col, targetbeam5Col, targetbeam6Col,
		startTimeCol, unixEpochDateTimeCol, numCols };

	MysqlQuery query(dbParameters_.getDb());
	query.execute(sqlstmt.str(), numCols, __FILE__, __LINE__);

	MYSQL_ROW row = mysql_fetch_row(query.getResultSet());
	if (mysql_num_rows(query.getResultSet()) > 1)
	{
		errorMsg << methodName <<  "found multiple rows of info: "
			<< prevActivityId << endl;
		throw SseException(errorMsg.str(), __FILE__, __LINE__,
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	if (!row) 
	{
		errorMsg <<  methodName << " act id: " << prevActivityId
			<< " info not found." << endl;
		throw SseException(errorMsg.str(), __FILE__, __LINE__,
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	// copy the target id for each beam from the database to
	// the act parameters

	actParameters_.setTargetIdForBeam(
			"beam1", query.getInt(row, targetbeam1Col, __FILE__, __LINE__));

	actParameters_.setTargetIdForBeam(
			"beam2", query.getInt(row, targetbeam2Col, __FILE__, __LINE__));

	actParameters_.setTargetIdForBeam(
			"beam3", query.getInt(row, targetbeam3Col, __FILE__, __LINE__));

	actParameters_.setTargetIdForBeam(
			"beam4", query.getInt(row, targetbeam4Col, __FILE__, __LINE__));

	actParameters_.setTargetIdForBeam(
			"beam5", query.getInt(row, targetbeam5Col, __FILE__, __LINE__));

	actParameters_.setTargetIdForBeam(
			"beam6", query.getInt(row, targetbeam6Col, __FILE__, __LINE__));

	prevActStartTime_ = query.getInt(row, startTimeCol, __FILE__, __LINE__);

	// Verify that the database is running on UTC time, since the 
	// activity assumes that date-times in ISO string format and
	// unix timestamps are both in UTC, and are interchangeable.

	string unixEpochDateTimeFromDb(query.getString(row, unixEpochDateTimeCol,
				__FILE__, __LINE__));
	string unixUtcEpoch("1970-01-01 00:00:00");

	if (unixEpochDateTimeFromDb != unixUtcEpoch)
	{
		throw SseException("database is not set to UTC time\n",
				__FILE__, __LINE__,
				SSE_MSG_ACT_FAILED, SEVERITY_ERROR);   
	}

}


double ObserveActivityImp::calculateSynthBeamsizeRads(double skyFreqMhz)
{
	double beamsizeRads = AtaInformation::ataBeamsizeRadians(
			skyFreqMhz, tscopeParameters_.getBeamsizeAtOneGhzArcSec());

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " 
			<< "beamsizeRads @ " << skyFreqMhz
			<< " MHz=" << beamsizeRads << endl;);

	return beamsizeRads;
}

double ObserveActivityImp::calculatePrimaryBeamsizeRads(double skyFreqMhz)
{
	const double arcSecPerDeg(3600);
	const double primaryBeamsizeAtOneGhzArcSec(
			tscopeParameters_.getPrimaryFovAtOneGhzDeg() * arcSecPerDeg);

	double beamsizeRads(AtaInformation::ataBeamsizeRadians(
				skyFreqMhz, primaryBeamsizeAtOneGhzArcSec));

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " 
			<< "primary beamsizeRads @ " << skyFreqMhz
			<< " MHz=" << beamsizeRads << endl;);

	return beamsizeRads;
}


/*
   Tune the ATA RF tunings using the telescope interface,
   and possibly point the telescope beams at target(s).
   The tscopeReady() method is called by the tscopeProxy
   when the commands have been fulfilled.
   */
void ObserveActivityImp::startTelescopes()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"startTelescopes()" << endl;);

	if (activityWrappingUp_.get())
	{
		return;
	}

	determineAntLists();

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Preparing telescopes..." << endl;

	startWatchdogTimer(tscopeReadyTimeout_,
			&ObserveActivityImp::handleTscopeReadyTimeout,
			actParameters_.getTscopeReadyTimeoutSecs());

	TscopeList &tscopeList = getAllNeededTscopes();

	// must set the count before starting them because components may
	// report in as ready before the loop finishes
	nTscopesStarted_ = tscopeList.size();

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"startTelescopes() nscopes: " << tscopeList.size() << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": " <<
			"startTelescopes() nscopes: " << tscopeList.size() << endl;
	try 
	{
		for (TscopeList::iterator it = tscopeList.begin(); 
				it != tscopeList.end(); ++it)
		{
			TscopeProxy *tscopeProxy = *it;

			if (!tscopeProxy->isAlive()) 
			{
				const string msg("Tscope no longer connected");
				throw SseException(msg, __FILE__, __LINE__,
						SSE_MSG_ACT_FAILED, SEVERITY_ERROR);
			}

			tscopeProxy->attachObserveActivity(this);

			tscopeProxy->beginSendingCommandSequence();

			if (useRf())
			{
				commandAtaTuningsForPreludeBeamsInUse(ataTuningToPreludeBeamsMultiMap_,
						tscopeProxy);
			}

			if (observingTargets())
			{
				SseArchive::SystemLog() << "Act " << getId() << ": " 
					<< "Pointing Beams..."  << endl; 

				pointBeams(tscopeProxy);

				SseArchive::SystemLog() << "Act " << getId() << ": "
					<< "Waiting for tscope ready..." << endl;

			}

			//JR - Tscope Setup
			if (isTscopeSetupActivity())
			{
				tscopeSetup(tscopeProxy);
			}
			else if (isAutoselectAntsActivity())
			{
				sendAutoselectAntsCommands(tscopeProxy);
			}
			else if (isPrepareAntsActivity())
			{
				sendPrepareAntsCommands(tscopeProxy);
			}
			else if (isFreeAntsActivity())
			{
				sendFreeAntsCommands(tscopeProxy);
			}
			else if (isBeamformerResetActivity())
			{
				beamformerReset(tscopeProxy);
			}
			else if (isBeamformerInitActivity())
			{
				beamformerInit(tscopeProxy);
			}
			else if (isBeamformerAutoattenActivity())
			{
				beamformerAutoatten(tscopeProxy);
			}
			else if (isPointAntsAndWaitActivity())
			{
				pointAntsAndWait(tscopeProxy);
			}

			tscopeProxy->requestStatusUpdate();

			tscopeProxy->doneSendingCommandSequence();

		}

	}
	catch (SseException & except)
	{
		stringstream strm;

		strm << "startTelescopes() failed: " << except.descrip();
		SseMessage::log(MsgSender, getId(),
				except.code(), except.severity(), strm.str(),
				except.sourceFilename(), except.lineNumber());

		terminateActivity("Could not start telescopes");
		return;  

	}

}

void ObserveActivityImp::determineAntLists()
{
	if (tscopeParameters_.isAntListSourceParam())
	{
		// Use the ant lists in the parameters

		xpolAnts_ = tscopeParameters_.getXpolAnts();
		ypolAnts_ = tscopeParameters_.getYpolAnts();
		primaryAnts_ = tscopeParameters_.getPrimaryAnts();

		/*
		   Make sure that the xpol and ypol ant lists 
		   are a subset of the primary beam list,
		   and that no list has duplicate entries.
		   */
		string xpolListName("xpol");
		vector<string> xpolVect(prepareAntList(xpolListName, xpolAnts_));

		string ypolListName("ypol");
		vector<string> ypolVect(prepareAntList(ypolListName, ypolAnts_));

		string primaryListName("primary");
		vector<string> primaryVect(prepareAntList(primaryListName, primaryAnts_));

		checkAntListSubset(xpolListName, xpolVect,
				primaryListName, primaryVect);

		checkAntListSubset(ypolListName, ypolVect,
				primaryListName, primaryVect);
	}
	else
	{
		// Use the predefined groups of ants

		const string antgroupKeyword("antgroup");
		xpolAnts_ = antgroupKeyword;
		ypolAnts_ = antgroupKeyword;
		primaryAnts_ = antgroupKeyword;
	}
} 

/*
   Split antlist by commas into separate elements.
   Sort them.  Remove and report duplicates.
   */
vector<string> ObserveActivityImp::
prepareAntList(const string & listName, const string & antList)
{
	string errorText;
	if (!SseCommUtil::validAtaSubarray(antList, errorText))
	{
		stringstream errorMsg;
		errorMsg << "Invalid subarray for " << listName << ".  "
			<< errorText << "\n";
		throw SseException(errorMsg.str(), __FILE__, __LINE__,
				SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
	}

	string delimit(",");
	vector<string> vect = SseUtil::tokenize(antList, delimit); 
	sort(vect.begin(), vect.end());

	// Check for duplicates.  Multiple dupes are reported once.
	if (!vect.empty())
	{
		stringstream dupeStrm;
		string lastDupe("");
		for (unsigned int i=0; i<vect.size()-1; ++i)
		{
			if (vect[i] == vect[i+1])
			{
				if (lastDupe != vect[i])
				{
					dupeStrm << vect[i] << " ";
					lastDupe = vect[i];
				}
			}
		}

		if (dupeStrm.str() != "")
		{
			stringstream strm;

			strm << "Warning: " << listName << " ant list"
				<< " has these duplicate ants: " 
				<< dupeStrm.str() << endl;

			SseMessage::log(MsgSender, getId(), SSE_MSG_INVALID_PARMS,
					SEVERITY_WARNING, strm.str(),
					__FILE__, __LINE__);

			// remove dupes so they are not reported again in any
			// later error messages

			vect.erase(unique(vect.begin(), vect.end()), vect.end());
		}
	}

	return vect;
}

/*
   Make sure the subsetList of ants is fully contained
   within the masterList.  The lists are assumed to be sorted.
   */
void ObserveActivityImp::checkAntListSubset(
		const string &subsetListName, 
		vector<string> & subsetList,
		const string & masterListName,
		vector<string> & masterList)
{
	vector<string> mismatchAnts;
	set_difference(subsetList.begin(), subsetList.end(),
			masterList.begin(), masterList.end(),
			back_inserter(mismatchAnts));

	if (mismatchAnts.size() > 0)
	{
		stringstream strm;

		strm << "These ants appear in the " << subsetListName
			<< " ant list that are not in the " << masterListName
			<< " ant list: ";
		for (unsigned int i=0; i<mismatchAnts.size(); ++i)
		{
			strm << mismatchAnts[i] << " ";
		}
		strm << endl;

		throw SseException(strm.str(), __FILE__, __LINE__,
				SSE_MSG_INVALID_PARMS, SEVERITY_ERROR); 
	}
}


/*
   Send sequence of commands to select ants for observing.
   */
void ObserveActivityImp::sendAutoselectAntsCommands(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"sendAutoselectAntsCommands()" << endl;);

	vector<string> preludeBeamsToUse(schedulerParameters_.getBeamsToUse());
	string beamList = "";
	for (vector<string>::const_iterator it = preludeBeamsToUse.begin();
	     it != preludeBeamsToUse.end(); ++it)
	{
	  const string & beamName(*it);
	  int beamNumber = beamNameToNumber(beamName);
	  if(beamList.size() != 0) beamList += ",";
	  beamList += SseUtil::intToStr(beamNumber);
    }

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Sending antgroup autoselect command. " 
		<< " bflist: " << beamList << endl;

	//tscopeProxy->antgroupAutoselect(tscopeParameters_.getMaxSefdJy(), obsFreqMhz);
	tscopeProxy->antgroupAutoselect(beamList);

	// request information about the selected ants so that it gets logged
	// JR - Do not request this information, is sent automatically noe
	//tscopeProxy->sendBackendCommand("antgroup list all");

	// tscopeReady will be called when cmd is complete
}

/*
   Tscope Setup
   */
void ObserveActivityImp::tscopeSetup(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"tscopeSetup()" << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Connecting to Tscope. " 
		<< endl;

	//Do the setup
	tscopeProxy->connect();


}



/*
   Send sequence of commands to prepare ants for observing.
   */
void ObserveActivityImp::sendPrepareAntsCommands(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"sendPrepareAntsCommands()" << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Subarray: " << primaryAnts_ << endl;

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Allocating subarray" << endl;
	tscopeProxy->allocate(primaryAnts_);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Turning on LNAs for subarray" << endl;
	tscopeProxy->lnaOn(primaryAnts_);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Setting PAMs for subarray" << endl;
	tscopeProxy->pamSet(primaryAnts_);

	/*
	   Assumes endObsFreqMhz is correctly set for highest desired obs
	   freq, regardless of whether rfTune is set to auto or user.
	   */
	double maxObsFreqMhz(schedulerParameters_.getEndObsFreqMhz());
	double ataFocusAdjustFactor(1.1);  // for optimal focus position
	double focusFreqMhz = maxObsFreqMhz * ataFocusAdjustFactor;

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Max obs freq: " << maxObsFreqMhz << " MHz. "
		<< "Setting feed focus on subarray to " 
		<< focusFreqMhz << " MHz" << endl;

	tscopeProxy->zfocus(primaryAnts_, focusFreqMhz);


	// TBD - wait for some kind of ready response?
	// tscopeProxy->requestReady();

	// temp debug - send ready directly
	tscopeReady(tscopeProxy);
}


/*
   Send sequence of commands to restore ants to nominal state
   */
void ObserveActivityImp::sendFreeAntsCommands(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"sendFreeAntsCommands()" << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Subarray: " << primaryAnts_ << endl;

	double focusFreqMhz(1420);   
	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Setting feed focus on subarray to " 
		<< focusFreqMhz << " MHz" << endl;
	tscopeProxy->zfocus(primaryAnts_, focusFreqMhz);

	// point subarray at "park" position
	// TBD get args from params
    // 0,41 is summer position, 180,18 is winter.
	//double azDeg = 0;
	//double elDeg = 41;
	double azDeg = 180;
	double elDeg = 18;

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Slewing subarray to az el: " 
		<< azDeg << " " << elDeg << endl;

	pointAtaSubarrayAzEl(tscopeProxy, primaryAnts_, azDeg, elDeg);


	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Deallocating subarray" << endl;
	tscopeProxy->deallocate(primaryAnts_);

	// no need to wait for slew to complete
	tscopeReady(tscopeProxy);
}


void ObserveActivityImp::beamformerReset(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"beamformerReset()" << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Resetting beamformer(s)" << endl;

	tscopeProxy->beamformerReset();

	// tscopeReady will be called when cmd is complete
}

void ObserveActivityImp::beamformerInit(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"beamformerInit()" << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Initializing beamformer(s) with antlists" << endl;

	// assign ants to beams
	vector<string> preludeBeamsToUse(schedulerParameters_.getBeamsToUse());
	assignSubarraysToAtaBeams(tscopeProxy, preludeBeamsToUse);

	//JR - beamformerInit starts the bfinit program. But first we need
	//to assign IP addresses for the --ip0 and --ip1 arguments to  bfinit
	for (vector<string>::const_iterator it = preludeBeamsToUse.begin();
			it != preludeBeamsToUse.end(); ++it)
	{
		const string & beamName(*it);

		vector<string> beamNameList;
		beamNameList.push_back(beamName);
		int beamNumber = beamNameToNumber(beamName);

		stringstream command;

		//Reset the bfinit destinations to nothing
		command << "BF DEST " << beamNumber << ",0,0:0" ;
		SseArchive::SystemLog() << "Act " << getId() << ": " 
			<< "Clearing beamformer(s) Dest:" << command.str() << endl;
		tscopeProxy->beamformerDest(command.str());
		command.str("");
		command << "BF DEST " << beamNumber << ",1,0:0" ;
		SseArchive::SystemLog() << "Act " << getId() << ": " 
			<< "Clearing beamformer(s) Dest:" << command.str() << endl;
		tscopeProxy->beamformerDest(command.str());

		//Loop through each channelizer assigning the Destination IP and port
		ChannelizerList channelizerList = getNssComponentTree()->getChansForBeams(beamNameList);
		for(ChannelizerList::iterator index = channelizerList.begin();
				index != channelizerList.end(); ++index)
		{
			ChannelizerProxy *proxy = *index;

			ssechan::Intrinsics chanIntrinsics = proxy->getIntrinsics();

			//Jane states that the address should not be the multicast address
			//that is secified in the startup files! Rather, the "host"
			//intrinsic value should be used.
			string address = SseCommUtil::getHostIpAddr((string)chanIntrinsics.host + "-data");

			SseArchive::SystemLog() << "Channelizer Hostname=" << chanIntrinsics.host << endl;

			command.str("");
			command << "BF DEST " << beamNumber << "," 
				<< chanIntrinsics.pol << ","
				//<< chanIntrinsics.beamBase.addr << ":" 
				<< address << ":" 
				<< chanIntrinsics.beamBase.port;
			SseArchive::SystemLog() << "Act " << getId() << ": " 
				<< "Initializing beamformer(s) Dest:" << command.str() << endl;
			tscopeProxy->beamformerDest(command.str());
		}
	}


	//Finally, start up bfinit.
	tscopeProxy->beamformerInit();

	// tscopeReady will be called when cmd is complete
}




void ObserveActivityImp::beamformerAutoatten(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"beamformerAutoatten()" << endl;);

	// assign ants to beams so that status can be read back
	vector<string> preludeBeamsToUse(schedulerParameters_.getBeamsToUse());
	assignSubarraysToAtaBeams(tscopeProxy, preludeBeamsToUse);

	// point subarray at dark sky
	// TBD get args from params
	double azDeg = 330;
	double elDeg = 30;

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Slewing subarray to az el: " 
		<< azDeg << " " << elDeg << endl;

	pointAtaSubarrayAzEl(tscopeProxy, primaryAnts_, azDeg, elDeg);

	SseArchive::SystemLog() << "Act " << getId() << ": " 
		<< "Waiting for subarray to reach position before "
		<< "requesting beamformer autoatten..." 
		<< endl;

	/*
	   Tscope is expected to defer execution of this command
	   until primary beam is on target.
	   */
	tscopeProxy->beamformerAutoatten();

	// tscopeReady will be called when cmd is complete
}


void ObserveActivityImp::pointAntsAndWait(TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"pointAntsAndWait()" << endl;);

	// assign ants to beams so that primary beam "ready" status can be determined
	vector<string> preludeBeamsToUse(schedulerParameters_.getBeamsToUse());
	assignSubarraysToAtaBeams(tscopeProxy, preludeBeamsToUse);

	// Determine primary beam position.
	calculatePrimaryBeamPointing(&primaryCenterRaRads_,
			&primaryCenterDecRads_);

	preparePrimaryBeam(tscopeProxy, primaryCenterRaRads_, primaryCenterDecRads_);
}


// Point the synthesized (and possibly primary) beams at
// the selected targets.

void ObserveActivityImp::pointBeams(TscopeProxy *tscopeProxy)
{
	/* For each prelude beam in use, get the associated ata beam(s)
	 * and point them at the appropriate target.
	 * Eg, prelude 'beam1' may be associated with
	 * ata beams 'beamxa1' and 'beamya1'.
	 */

	vector<string> preludeBeamsToUse(schedulerParameters_.getBeamsToUse());
	vector<TargetInfo> nominalTargets;
	getNominalTargetPositionsForPreludeBeams(preludeBeamsToUse, 
			nominalTargets);

	// save the first tune freq assigned for later use 
	Assert(! nominalTargets.empty());
	double tuningSkyFreqMhz(nominalTargets[0].tuningSkyFreqMhz);

	// Determine primary beam position.
	calculatePrimaryBeamPointing(&primaryCenterRaRads_,
			&primaryCenterDecRads_);

	double primaryBeamsizeRads(calculatePrimaryBeamsizeRads(
				tuningSkyFreqMhz));
	logBeamsize(PrimaryBeam, primaryBeamsizeRads);

	double synthBeamsizeRads(calculateSynthBeamsizeRads(
				tuningSkyFreqMhz));
	logBeamsize("synthesized", synthBeamsizeRads);

	// adjust the nominal pointings, as needed for offs, etc.
	vector<TargetInfo> adjustedTargets;
	if (isOffObservation())
	{
		if (actParameters_.getOffActNullType() == ActParameters::NULL_AXIAL)
		{
			SseArchive::SystemLog() 
				<< "Act " << getId() << ": "
				<< "Putting axial nulls on target coordinates"
				<< " instead of moving beams off source"
				<< endl;

			adjustedTargets = nominalTargets;
		}
		else
		{
			findOffPositions(nominalTargets, 
					primaryBeamsizeRads, synthBeamsizeRads,
					primaryCenterRaRads_, primaryCenterDecRads_,
					adjustedTargets);
		}
	}
	else if (isGridWestObservation() || isGridEastObservation()
			|| isGridNorthObservation() || isGridSouthObservation())
	{
		findGridPositions(nominalTargets, synthBeamsizeRads,
				adjustedTargets);
	}
	else
	{
		// no target position changes
		adjustedTargets = nominalTargets;
	}

	/*
TBD: assignSubarraysToAtaBeams really only needs to be 
done once during an observing session.  Consider deleting
it from here.
*/
	assignSubarraysToAtaBeams(tscopeProxy, preludeBeamsToUse);

	assignTargetsToAtaBeams(tscopeProxy, adjustedTargets);

	prepareNullBeams(tscopeProxy, nominalTargets, adjustedTargets);

	pointBeamformer(tscopeProxy);

	preparePrimaryBeam(tscopeProxy, primaryCenterRaRads_, primaryCenterDecRads_);
}


void ObserveActivityImp::logBeamsize(const string &beamType,
		double beamsizeRads)
{
	SseArchive::SystemLog()
		<< "Act " << getId() << ": " 
		<< beamType << " beamsize: " << beamsizeRads << " rads, "
		<< SseAstro::radiansToArcSecs(beamsizeRads) << " arcsec, "
		<< SseAstro::radiansToDegrees(beamsizeRads) << " deg " 
		<< endl;
}

/* Look up the target position assigned to each beam, and log 
   its coordinates.  Returns results in the nominalTargetPositions
   container.
   */
void ObserveActivityImp::getNominalTargetPositionsForPreludeBeams(
		const vector<string> & preludeBeamsToUse, 
		vector<TargetInfo> & nominalTargetPositions)
{
	for (vector<string>::const_iterator it = preludeBeamsToUse.begin();
			it != preludeBeamsToUse.end(); ++it)
	{
		const string & preludeBeamName(*it);

		TargetInfo targetInfo;
		targetInfo.preludeBeamName = preludeBeamName;
		targetInfo.targetId = actParameters_.getTargetIdForBeam(preludeBeamName);
		targetInfo.tuningSkyFreqMhz = determineTuningSkyFreqMhzForPreludeBeam(
				preludeBeamName);

		bool movingTarget(false);
		lookUpTargetCoordinates(dbParameters_.getDb(),
				targetInfo.targetId, &targetInfo.coords.ra2000Rads,
				&targetInfo.coords.dec2000Rads, &movingTarget);

		stringstream descripStrm;
		descripStrm << preludeBeamName << " TargetId: " 
			<< targetInfo.targetId << " ";

		if (movingTarget)
		{
			descripStrm << "Moving Target ";
		}
		logPosition(descripStrm.str(), targetInfo.coords.ra2000Rads,
				targetInfo.coords.dec2000Rads);

		nominalTargetPositions.push_back(targetInfo);

	}

	Assert(preludeBeamsToUse.size() == nominalTargetPositions.size());

}

/*
   Find off positions for all the targets in the nominalTargetPositions
   container.  Results are returned in the adjustedTargetPositions 
   container.
   */
void ObserveActivityImp::findOffPositions(
		const vector<TargetInfo> & nominalTargetPositions,
		double primaryBeamsizeRads,
		double synthBeamsizeRads,
		double primaryCenterRaRads,
		double primaryCenterDecRads,
		vector<TargetInfo> & adjustedTargetPositions)
{
	double minBeamSepFactor(schedulerParameters_.getMinTargetSepBeamsizes());

	auto_ptr<OffPositions> offPositions(new OffPositions(
				synthBeamsizeRads, minBeamSepFactor,
				primaryCenterRaRads, primaryCenterDecRads,
				primaryBeamsizeRads));

	vector<OffPositions::Position> targets;
	vector<OffPositions::Position> offs;

	// prepare the targets list 
	for (vector<TargetInfo>::const_iterator it =
			nominalTargetPositions.begin();
			it != nominalTargetPositions.end(); ++it)
	{
		const TargetInfo & targetInfo(*it);

		targets.push_back(OffPositions::Position(
					targetInfo.coords.ra2000Rads, targetInfo.coords.dec2000Rads));
	}

	int deltaTimeBetweenObsSecs = calculateDeltaTimeBetweenObsSecs(
			prevActStartTime_);

	offPositions->getOffPositions(targets, deltaTimeBetweenObsSecs,
			offs);
	Assert(targets.size() == offs.size());

	// fill adjusted targets list using the nominal & off position info
	vector<OffPositions::Position>::const_iterator itOffs = offs.begin();

	for (vector<TargetInfo>::const_iterator itNominal = nominalTargetPositions.begin();
			itNominal != nominalTargetPositions.end() && itOffs != offs.end(); 
			++itNominal, ++itOffs)
	{
		const TargetInfo & nominalTargetInfo(*itNominal);
		TargetInfo adjTargetInfo(nominalTargetInfo);
		const OffPositions::Position & offPos(*itOffs);

		adjTargetInfo.coords.ra2000Rads = offPos.raRads_;
		adjTargetInfo.coords.dec2000Rads = offPos.decRads_;

		stringstream descripStrm;
		descripStrm << "OFF position for " << adjTargetInfo.preludeBeamName 
			<< " :";
		logPosition(descripStrm.str(), adjTargetInfo.coords.ra2000Rads,
				adjTargetInfo.coords.dec2000Rads);

		adjustedTargetPositions.push_back(adjTargetInfo);
	}
	Assert(nominalTargetPositions.size() == adjustedTargetPositions.size());

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< *offPositions << endl;	 

}


/*
   Find hpbw offset positions for all the targets in the nominalTargetPositions
   container.  Results are returned in the adjustedTargetPositions container.
   For now, all targets are offset in the same direction (north, south, east
   or west).  
TBD: use multiple beams to point in different directions at the same time.
*/

void ObserveActivityImp::findGridPositions(
		const vector<TargetInfo> & nominalTargetPositions,
		double synthBeamsizeRads,
		vector<TargetInfo> & adjustedTargetPositions)
{
	double beamOffsetFactor(0.5);  // half power point.  tbd make a parameter
	double distToMoveRads(synthBeamsizeRads * beamOffsetFactor);

	for (vector<TargetInfo>::const_iterator it = nominalTargetPositions.begin();
			it != nominalTargetPositions.end(); ++it)
	{
		const TargetInfo & nominalTargetInfo(*it);
		TargetInfo adjTargetInfo(nominalTargetInfo);

		string direction("");
		if (isGridWestObservation())
		{
			direction = "West";
			OffPositions::moveWest(nominalTargetInfo.coords.ra2000Rads,
					nominalTargetInfo.coords.dec2000Rads,
					distToMoveRads,
					&adjTargetInfo.coords.ra2000Rads,
					&adjTargetInfo.coords.dec2000Rads);
		}
		else if (isGridEastObservation())
		{
			direction = "East";
			OffPositions::moveEast(nominalTargetInfo.coords.ra2000Rads,
					nominalTargetInfo.coords.dec2000Rads,
					distToMoveRads,
					&adjTargetInfo.coords.ra2000Rads,
					&adjTargetInfo.coords.dec2000Rads);
		}
		else if (isGridNorthObservation())
		{
			direction = "North";
			OffPositions::moveNorth(nominalTargetInfo.coords.ra2000Rads,
					nominalTargetInfo.coords.dec2000Rads,
					distToMoveRads,
					&adjTargetInfo.coords.ra2000Rads,
					&adjTargetInfo.coords.dec2000Rads);
		}
		else if (isGridSouthObservation())
		{
			direction = "South";
			OffPositions::moveSouth(nominalTargetInfo.coords.ra2000Rads,
					nominalTargetInfo.coords.dec2000Rads,
					distToMoveRads,
					&adjTargetInfo.coords.ra2000Rads,
					&adjTargetInfo.coords.dec2000Rads);
		}
		else 
		{
			AssertMsg(0, "no grid direction set");
		}

		stringstream descripStrm;
		descripStrm << direction << " position for " 
			<< adjTargetInfo.preludeBeamName 
			<< " :";
		logPosition(descripStrm.str(), adjTargetInfo.coords.ra2000Rads,
				adjTargetInfo.coords.dec2000Rads);

		adjustedTargetPositions.push_back(adjTargetInfo);
	}
	Assert(nominalTargetPositions.size() == adjustedTargetPositions.size());

}

void ObserveActivityImp::prepareNullBeams(TscopeProxy *tscopeProxy,
		const vector<TargetInfo> & nominalTargets,
		const vector<TargetInfo> & adjustedTargets)
{
	TscopeNullType nullType;
	nullType.nullType = TscopeNullType::NONE;

	if (isOffObservation())
	{
		if (actParameters_.getOffActNullType() == ActParameters::NULL_AXIAL)
		{
			// Put an axial null on the nominal target position
			nullType.nullType = TscopeNullType::AXIAL;
		}
		else if (actParameters_.getOffActNullType() == ActParameters::NULL_PROJECTION)
		{
			nullType.nullType = TscopeNullType::PROJECTION;
			assignOffActProjectionNulls(tscopeProxy, nominalTargets);
		}
		else
		{
			Assert(actParameters_.getOffActNullType() == ActParameters::NULL_NONE);
		}
	}
	else if (isMultitargetObservation() &&
			actParameters_.useMultiTargetNulls())
	{
		nullType.nullType = TscopeNullType::PROJECTION;
		assignCrossBeamProjectionNulls(tscopeProxy, adjustedTargets);
	}

	tscopeProxy->beamformerSetNullType(nullType);
}


	vector<string>
ObserveActivityImp::getAtaBeamsForPreludeBeam(const string & preludeBeamName)
{
	vector<string> ataBeams(expectedTree_->getAtaBeamsForBeam(preludeBeamName));
	if (ataBeams.size() == 0)
	{
		stringstream errorMsg;
		errorMsg << "No ATA beam(s) are assigned to beam '" 
			<< preludeBeamName 
			<< "' in expected components config file\n";

		throw SseException(errorMsg.str(), __FILE__, __LINE__,
				SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
	}

	return ataBeams;
}

/*
   Assign the coords as a NULL to all the ATA beams associated with the
   specified prelude beam.
   */
void ObserveActivityImp::addNullBeamCoordsToAtaBeams(
		TscopeProxy * tscopeProxy,
		const string & preludeBeamName,
		TscopeBeamCoords &coords)
{
	vector<string> ataBeamNames(getAtaBeamsForPreludeBeam(
				preludeBeamName));
	Assert(ataBeamNames.size() > 0);

	for (unsigned int ataBeam=0; ataBeam < ataBeamNames.size(); ++ataBeam)
	{
		TscopeBeam ataBeamNameCode = SseTscopeMsg::nameToBeam(ataBeamNames[ataBeam]);
		if (ataBeamNameCode == TSCOPE_INVALID_BEAM)
		{
			stringstream errorMsg;
			errorMsg << "invalid ATA beam name '" 
				<< ataBeamNames[ataBeam]
				<< "'\n";

			throw SseException(errorMsg.str(), __FILE__, __LINE__,
					SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
		}
		coords.beam = ataBeamNameCode;
		tscopeProxy->beamformerAddNullBeamCoords(coords);
	}
}

/*
   Put a projection null on each beam using the assigned coordinates.
   */
void ObserveActivityImp::assignOffActProjectionNulls(
		TscopeProxy *tscopeProxy,
		const vector<TargetInfo> & targets)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"assignOffActProjectionNulls()" << endl;);

	SseArchive::SystemLog() 
		<< "Act " << getId() << ": "
		<< "Using OFF act projection nulls." << endl;

	/*
	   Take each prelude beam in turn.  Assign its target coordinates
	   as a NULL on each ATA beam associated with it.
	   */
	tscopeProxy->beamformerClearBeamNulls();

	TscopeBeamCoords coords;
	coords.pointing.coordSys = TscopePointing::J2000;

	for (unsigned int preludeBeamIt=0; preludeBeamIt <targets.size();
			++preludeBeamIt)
	{
		coords.pointing.raHours = SseAstro::radiansToHours(
				targets[preludeBeamIt].coords.ra2000Rads);

		coords.pointing.decDeg = SseAstro::radiansToDegrees(
				targets[preludeBeamIt].coords.dec2000Rads);

		addNullBeamCoordsToAtaBeams(
				tscopeProxy, targets[preludeBeamIt].preludeBeamName,
				coords);
	}
}

/*
   For each prelude beam in use:
   Assign its target coordinates to each ATA beam on the
   other prelude beams in use.
   */
void ObserveActivityImp::assignCrossBeamProjectionNulls(
		TscopeProxy *tscopeProxy,
		const vector<TargetInfo> & targets)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"assignCrossBeamProjectionNulls()" << endl;);

	SseArchive::SystemLog() 
		<< "Act " << getId() << ": "
		<< "Using multitarget projection nulls." << endl;

	/*
	   Take each prelude beam in turn.  Assign its target coordinates
	   as a NULL on each ATA beam associated with the other prelude beams.
	   */
	tscopeProxy->beamformerClearBeamNulls();

	TscopeBeamCoords coords;
	coords.pointing.coordSys = TscopePointing::J2000;

	for (unsigned int outerBeam = 0; outerBeam < targets.size();
			++outerBeam)
	{
		coords.pointing.raHours = SseAstro::radiansToHours(
				targets[outerBeam].coords.ra2000Rads);

		coords.pointing.decDeg = SseAstro::radiansToDegrees(
				targets[outerBeam].coords.dec2000Rads);

		for (unsigned int innerBeam = 0; innerBeam < targets.size();
				++innerBeam)
		{
			if (outerBeam != innerBeam)
			{
				addNullBeamCoordsToAtaBeams(
						tscopeProxy, targets[innerBeam].preludeBeamName,
						coords);
			}
		}
	}
}

/*
   Assign ant subarrays to the ata beams associated with each
   specified prelude beam.
   */
void ObserveActivityImp::assignSubarraysToAtaBeams(
		TscopeProxy *tscopeProxy,
		const vector<string> & preludeBeamsToUse)
{
	tscopeProxy->beamformerClearAnts();

	for (vector<string>::const_iterator it = preludeBeamsToUse.begin();
			it != preludeBeamsToUse.end(); ++it)
	{
		const string & preludeBeamName(*it);
		vector<string> ataBeams(getAtaBeamsForPreludeBeam(preludeBeamName));

		for (vector<string>::iterator ataBeamIt = ataBeams.begin();
				ataBeamIt != ataBeams.end(); ++ataBeamIt)
		{
			const string & ataBeamName(*ataBeamIt);

			// Assign subarray to ata synth beam based on its polarity.
			string subarray;
			string beamNameUppercase(SseUtil::strToUpper(ataBeamName));
			if (beamNameUppercase.find('X') != string::npos)
			{
				subarray = xpolAnts_;
			}
			else if (beamNameUppercase.find('Y') != string::npos)
			{
				subarray = ypolAnts_;
			}
			else
			{
				throw SseException(
						"beam name does not have X or Y in it, can't look up ants: "
						+ ataBeamName + "\n", __FILE__, __LINE__,
						SSE_MSG_ACT_FAILED, SEVERITY_ERROR);
			}

			assignAtaSubarrayToAtaBeam(tscopeProxy, ataBeamName, subarray);
		}
	}
}


/*
   Assign target coordinates to the ata synth beams that are
   associated with each prelude beam in the targets container. 
   */
void ObserveActivityImp::assignTargetsToAtaBeams(
		TscopeProxy *tscopeProxy,
		const vector<TargetInfo> & targets)
{
	tscopeProxy->beamformerClearCoords();

	for (vector<TargetInfo>::const_iterator it = targets.begin();
			it != targets.end(); ++it)
	{
		const TargetInfo & targetInfo(*it);
		const string & preludeBeamName(targetInfo.preludeBeamName);

		int preludeBeamNumber = beamNameToNumber(preludeBeamName);
		vector<string> ataBeams(getAtaBeamsForPreludeBeam(preludeBeamName));

		// make ataSynthBeam to subarray assignments
		stringstream ataBeamNamesUsed;
		for (vector<string>::iterator ataBeamIt = ataBeams.begin();
				ataBeamIt != ataBeams.end(); ++ataBeamIt)
		{
			const string & ataBeamName(*ataBeamIt);

			setAtaBeamCoords(tscopeProxy, ataBeamName, preludeBeamNumber, 
					targetInfo.coords.ra2000Rads, 
					targetInfo.coords.dec2000Rads);

			ataBeamNamesUsed << ataBeamName << " ";
		}

		SseArchive::SystemLog() 
			<< "Act " << getId() << ": "
			<< "TargetId: " << targetInfo.targetId
			<< " for beam: " <<  preludeBeamName 
			<< " on telescope: " << tscopeProxy->getName()
			<< " using ata beams: " << ataBeamNamesUsed.str()
			<< endl;
	}
}

void ObserveActivityImp::preparePrimaryBeam(
		TscopeProxy *tscopeProxy,
		double primaryCenterRaRads,
		double primaryCenterDecRads)
{
	string & subarray(primaryAnts_);

	if (actParameters_.pointPrimaryBeam())
	{
		// Acting as primary observer

		pointAtaSubarray(tscopeProxy, subarray,
				primaryCenterRaRads, 
				primaryCenterDecRads);
	}
	else
	{
		SseArchive::SystemLog()
			<< "Act " << getId() << ": " << "Using preassigned "
			<< "primary beam Ra/Dec coords" << endl;

		/*
		   This is a commensal observing mode, so
		   request that the tscope check that the primary beam
		   subarray is pointing where expected.
		   */
		requestAtaSubarrayPointingCheck(tscopeProxy,
				subarray,
				primaryCenterRaRads, 
				primaryCenterDecRads);
	}
}


void ObserveActivityImp::pointBeamformer(TscopeProxy *tscopeProxy)
{
	/*
	   Issue beamformer point or cal command, using
	   the previously assigned coordinates.  The tscope
	   is expected to defer execution of this command until
	   the primary beam is on target.
	   */
	if (isCalObservation())
	{
		TscopeCalRequest request(tscopeParameters_.getCalRequest());

		SseArchive::SystemLog() 
			<< "Act " << getId() << ": " << request;

		tscopeProxy->beamformerCal(request);
	}
	else
	{
		tscopeProxy->beamformerPoint();
	}
}


void ObserveActivityImp::assignAtaSubarrayToAtaBeam(
		TscopeProxy *tscopeProxy,
		const string &ataBeamName,
		const string &subarray)
{
	TscopeBeam ataBeamNameCode = SseTscopeMsg::nameToBeam(ataBeamName);
	if (ataBeamNameCode == TSCOPE_INVALID_BEAM)
	{
		stringstream errorMsg;
		errorMsg << "Could not assign subarray to beam: invalid ATA beam name '" 
			<< ataBeamName
			<< "'\n";

		throw SseException(errorMsg.str(), __FILE__, __LINE__,
				SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
	}

	// assume subarray has been validated

	TscopeAssignSubarray assignSub;
	assignSub.beam = ataBeamNameCode;
	SseUtil::strMaxCpy(assignSub.subarray, subarray.c_str(),
			MAX_TEXT_STRING);

	tscopeProxy->assignSubarray(assignSub);

}

double ObserveActivityImp::determineTuningSkyFreqMhzForPreludeBeam(
		const string & preludeBeamName)
{
	string ataTuningName = getTuningNameForPreludeBeam(preludeBeamName);
	if (ataTuningName == "")
	{
		throw SseException("failed to get ATA tuning name for prelude beam",
				__FILE__, __LINE__);
	}

	double tuningSkyFreqMhz = tscopeParameters_.getTuningSkyFreqMhz(
			SseUtil::strToLower(ataTuningName));

	return tuningSkyFreqMhz;
}

void ObserveActivityImp::setAtaBeamCoords(
		TscopeProxy *tscopeProxy, 
		const string &ataBeamName, int preludeBeamNumber,
		double ra2000Rads, double dec2000Rads)
{
	TscopeBeam ataBeamNameCode = SseTscopeMsg::nameToBeam(ataBeamName);
	if (ataBeamNameCode == TSCOPE_INVALID_BEAM)
	{
		stringstream errorMsg;
		errorMsg << "Could not point beam: invalid ATA beam name '" 
			<< ataBeamName
			<< "'\n";

		throw SseException(errorMsg.str(), __FILE__, __LINE__,
				SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
	}

	TscopeBeamCoords coords;
	coords.beam = ataBeamNameCode;
	coords.pointing.coordSys = TscopePointing::J2000;
	coords.pointing.raHours = SseAstro::radiansToHours(ra2000Rads);
	coords.pointing.decDeg = SseAstro::radiansToDegrees(dec2000Rads);

	tscopeProxy->beamformerSetBeamCoords(coords);

	if (dbParameters_.useDb()) 
	{
		recordPointRequestInDatabase(preludeBeamNumber, ataBeamName, coords.pointing);
	}

	// remember which beams are in use for later logging
	ataBeamsInUse_.push_back(ataBeamNameCode);

}

void ObserveActivityImp::pointAtaSubarray(
		TscopeProxy *tscopeProxy, 
		const string &subarray, 
		double ra2000Rads, double dec2000Rads)
{
	// Assume subarray has already been validated

	TscopeSubarrayCoords coords;
	SseUtil::strMaxCpy(coords.subarray, subarray.c_str(),
			MAX_TEXT_STRING);
	coords.pointing.coordSys = TscopePointing::J2000;
	coords.pointing.raHours = SseAstro::radiansToHours(ra2000Rads);
	coords.pointing.decDeg = SseAstro::radiansToDegrees(dec2000Rads);

	tscopeProxy->pointSubarray(coords);

	if (dbParameters_.useDb())
	{
		int preludeBeamNumber=-1;  // fake value
		string beamName(PrimaryBeam);
		recordPointRequestInDatabase(preludeBeamNumber, beamName, coords.pointing);
	}

}

void ObserveActivityImp::pointAtaSubarrayAzEl(
		TscopeProxy *tscopeProxy, 
		const string &subarray, 
		double azDeg, double elDeg)
{
	// Assume subarray has already been validated

	TscopeSubarrayCoords coords;
	SseUtil::strMaxCpy(coords.subarray, subarray.c_str(),
			MAX_TEXT_STRING);
	coords.pointing.coordSys = TscopePointing::AZEL;
	coords.pointing.azDeg = azDeg;
	coords.pointing.elDeg = elDeg;

	tscopeProxy->pointSubarray(coords);

	if (dbParameters_.useDb())
	{
		int preludeBeamNumber=-1;  // fake value
		string beamName(PrimaryBeam);
		recordPointRequestInDatabase(preludeBeamNumber, beamName, coords.pointing);
	}

}


/*
   Activity did not point the primary beam, but needs
   to make sure that the beam is pointed where expected.
   Ask tscope to verify position.
   */
void ObserveActivityImp::requestAtaSubarrayPointingCheck(
		TscopeProxy *tscopeProxy, 
		const string &subarray, 
		double ra2000Rads, double dec2000Rads)
{
	// Assume subarray has already been validated

	TscopeSubarrayCoords coords;
	SseUtil::strMaxCpy(coords.subarray, subarray.c_str(),
			MAX_TEXT_STRING);
	coords.pointing.coordSys = TscopePointing::J2000;
	coords.pointing.raHours = SseAstro::radiansToHours(ra2000Rads);
	coords.pointing.decDeg = SseAstro::radiansToDegrees(dec2000Rads);

	tscopeProxy->requestPointingCheck(coords);

	int preludeBeamNumber=-1;  // fake value
	string beamName(PrimaryBeam);
	recordPointRequestInDatabase(preludeBeamNumber, beamName, coords.pointing);
}


void ObserveActivityImp::calculatePrimaryBeamPointing(
		double * ra2000Rads, double * dec2000Rads)
{
	TargetId targetId(-1);

	/* Primary beam position is specified either by a targetId
	   or by explicit RA/Dec coordinates */

	if (actParameters_.getPrimaryBeamPositionType() == 
			ActParameters::PRIMARY_BEAM_POS_TARGET_ID)
	{
		targetId = actParameters_.getPrimaryTargetId();
		bool movingTarget(false);
		lookUpTargetCoordinates(dbParameters_.getDb(), targetId,
				ra2000Rads, dec2000Rads, &movingTarget);
	}
	else 
	{
		Assert(actParameters_.getPrimaryBeamPositionType() ==
				ActParameters::PRIMARY_BEAM_POS_COORDS);

		*ra2000Rads = SseAstro::hoursToRadians(
				actParameters_.getPrimaryBeamRaHours());

		*dec2000Rads = SseAstro::degreesToRadians(
				actParameters_.getPrimaryBeamDecDeg());
	}

	stringstream descripStrm;
	descripStrm << PrimaryBeam << " TargetId: " 
		<< targetId << " ";

	logPosition(descripStrm.str(), *ra2000Rads, *dec2000Rads);

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "TargetId: " << targetId
		<< " for beam: " << PrimaryBeam
		<< endl;
}


/* Determine the freq for each ata tuning to be used.
   If autotune is on, then use the dx freqs on that tuning
   to determine the sky freq, otherwise use the already set tscope
   parameter.
   */
void ObserveActivityImp::determineAtaTuningFreqsForPreludeBeamsInUse(
		const AtaTuningToPreludeBeamsMultiMap & ataTuningToPreludeBeamsMultiMap)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"determineAtaTuningFreqsForPreludeBeamsInUse()" << endl;);

	// For each ATA tuning in use:
	for (int tuningIndex = 0; tuningIndex < TSCOPE_N_TUNINGS; ++tuningIndex)
	{
		TscopeTuning tuning(static_cast<TscopeTuning>(tuningIndex));
		if (ataTuningToPreludeBeamsMultiMap.find(tuning) != 
				ataTuningToPreludeBeamsMultiMap.end())
		{
			determineAtaTuning(tuning, ataTuningToPreludeBeamsMultiMap);
		}
	}

	// TBD error if no tunings are in use

}


/*
   For each ata tuning in use, look up the associated tscope parameter
   and send that sky freq to the telescope, including the baseband offset.
   */
void ObserveActivityImp::commandAtaTuningsForPreludeBeamsInUse(
		const AtaTuningToPreludeBeamsMultiMap & ataTuningToPreludeBeamsMultiMap,
		TscopeProxy *tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"commandAtaTuningsForPreludeBeamsInUse()" << endl;);

	// For each ATA tuning in use:
	for (int tuningIndex = 0; tuningIndex < TSCOPE_N_TUNINGS; ++tuningIndex)
	{
		TscopeTuning tuning(static_cast<TscopeTuning>(tuningIndex));
		if (ataTuningToPreludeBeamsMultiMap.find(tuning) != 
				ataTuningToPreludeBeamsMultiMap.end())
		{
			// Send tune command to telescope. Include baseband offset
			// so that the center of the ATA D/A band output appears
			// at the desired frequency.

			TscopeTuneRequest tuneRequest;
			tuneRequest.tuning = tuning;

			tuneRequest.skyFreqMhz = tscopeParameters_.getTuningSkyFreqMhz(
					SseUtil::strToLower(SseTscopeMsg::tuningToName(tuning)));

			tuneRequest.skyFreqMhz += tscopeParameters_.getBasebandTuneOffsetMhz();

			tscopeProxy->tune(tuneRequest);

			// save tuning for later logging
			ataTuningSkyFreqMhz_[tuning] = tuneRequest.skyFreqMhz;

			SseArchive::SystemLog() 
				<< "Act " << getId() << ": " 
				<< "Sending tuning request (including baseband tune offset of "
				<< tscopeParameters_.getBasebandTuneOffsetMhz() << " MHz) "
				<< "to telescope for " 
				<< SseTscopeMsg::tuningToName(tuning) << " of " 
				<< tuneRequest.skyFreqMhz << " MHz"
				<< endl; 

		}
	}

	// TBD error if no tunings are in use

}


// Fill in ataTuningToPreludeBeamsMultiMap for prelude beams in use
//
void ObserveActivityImp::findAtaTuningsForPreludeBeamsInUse(
		AtaTuningToPreludeBeamsMultiMap & ataTuningToPreludeBeamsMultiMap)
{
	/* For each prelude beam in use, get the associated ata beam(s)
	 * and extract the ata tuning.
	 * Eg, prelude 'beam1' may be associated with
	 *   ata beams 'beamxd1' and 'beamyd1'. The ATA RF tuning is specified
	 *   in the ATA beamname as the letter before the last number,
	 *   in this example, the associated ATA RF tuning is 'd'.
	 * Each prelude beam must have one (and only one) ATA RF tuning assigned to
	 * it, but note that multiple prelude beams can be associated with
	 * the same ATA RF tuning.
	 */

	vector<string> preludeBeamsToUse(schedulerParameters_.getBeamsToUse());

	for (vector<string>::iterator preludeBeamIt = preludeBeamsToUse.begin();
			preludeBeamIt != preludeBeamsToUse.end(); ++preludeBeamIt)
	{
		const string & preludeBeamName(*preludeBeamIt);

		vector<string> ataBeamsForPreludeBeam(
				getAtaBeamsForPreludeBeam(preludeBeamName));

		// Extract ata tuning from beam names.  Error if there isn't one,
		// or there's more than one (ie, ATA beams from different ATA tunings
		// were assigned to the same prelude beam).

		string lastShortTuningName("");
		for (vector<string>::iterator ataBeamNameIt = ataBeamsForPreludeBeam.begin();
				ataBeamNameIt != ataBeamsForPreludeBeam.end(); ++ataBeamNameIt)
		{
			const string & ataBeamName(*ataBeamNameIt);

			// extract the tuning name
			const int tuningNameIndex(5);  // beam{x|y}{tuningName}{number}
			if (ataBeamName.length() < tuningNameIndex+1)
			{
				stringstream errorMsg;
				errorMsg << "Error extracting ATA RF tuning from ATA beam name '"
					<< ataBeamName 
					<< "' for prelude beam '" << preludeBeamName
					<< "' in expected components config file, "
					<< "ata beam name is too short\n";

				throw SseException(errorMsg.str(), __FILE__, __LINE__,
						SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
			}
			const int tuningNameLength(1);
			string shortTuningName(ataBeamName.substr(tuningNameIndex,tuningNameLength));


#if 0
			cout << "tuning name for beam " << ataBeamName << " is " 
				<< shortTuningName << endl;
#endif

			// store prelude beam associated with ata tuning
			string ataTuningFullName("TUNING" + SseUtil::strToUpper(shortTuningName));
			TscopeTuning tuning = SseTscopeMsg::nameToTuning(ataTuningFullName);
			if (tuning == TSCOPE_INVALID_TUNING)
			{
				stringstream errorMsg;
				errorMsg << "Error extracting ATA RF tuning from ATA beam name '"
					<< ataBeamName 
					<< "' for prelude beam '" << preludeBeamName
					<< "' in expected components config file, "
					<< "invalid tuning of '" 
					<< shortTuningName << "' specified in beam name\n";

				throw SseException(errorMsg.str(), __FILE__, __LINE__,
						SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  

			}

			// check that all ata tunings are the same for all atabeams 
			// on this prelude beam
			if (lastShortTuningName != "")
			{
				if (lastShortTuningName != shortTuningName)
				{
					stringstream errorMsg;
					errorMsg << "Error extracting ATA RF tuning from ATA beam names "
						<<  "for prelude beam '" << preludeBeamName
						<< "' in expected components config file, "
						<< " ATA tunings do not match: '" 
						<< lastShortTuningName << "' and '" << shortTuningName 
						<< "'\n";

					throw SseException(errorMsg.str(), __FILE__, __LINE__,
							SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  

				}
			}
			lastShortTuningName = shortTuningName;

			storeTuningNameForPreludeBeam(preludeBeamName, ataTuningFullName);
			ataTuningToPreludeBeamsMultiMap.insert(make_pair(tuning, preludeBeamName));

		}
	}

}



/* 
   If in auto tune mode, set the telescope tuning parameter associated
   with the given ata 'tuning', otherwise use the existing value.
   Verify min/max dx tuning range is within the input bandwidth limits.
   */

void ObserveActivityImp::determineAtaTuning(
		TscopeTuning tuning,
		const AtaTuningToPreludeBeamsMultiMap &ataTuningToPreludeBeamsMultiMap)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"determineAtaTuning()" << endl;);

	const string ataTuningName(SseUtil::strToLower(
				SseTscopeMsg::tuningToName(tuning)));

	// get the prelude beams on this ata tuning
	vector<string> preludeBeams;
	AtaTuningToPreludeBeamsMultiMap::const_iterator it;
	for (it = ataTuningToPreludeBeamsMultiMap.lower_bound(tuning);
			it != ataTuningToPreludeBeamsMultiMap.upper_bound(tuning);
			++it)
	{
		const string & preludeBeamName(it->second);
		preludeBeams.push_back(preludeBeamName);
	}

	// remove duplicate prelude beams
	sort(preludeBeams.begin(), preludeBeams.end());
	preludeBeams.erase(unique(preludeBeams.begin(), preludeBeams.end()),
			preludeBeams.end());

	// save names of prelude beams
	stringstream preludeBeamNamesStrm;
	for (vector<string>::iterator pbnamesIt = preludeBeams.begin();
			pbnamesIt != preludeBeams.end(); ++pbnamesIt)
	{
		preludeBeamNamesStrm << *pbnamesIt << " ";
	}

	if (useDx())
	{
	double dxMinFreqMhz(-1);
	double dxMaxFreqMhz(-1);
	double dxBandwidthMhz(0);
	double dxMeanSkyFreqMhz(-1);

	ChannelizerList chanList = getNssComponentTree()->getChansForBeams(preludeBeams);

	if (chanList.size() == 0)
	{
		stringstream errorMsg;
		errorMsg << "No Channelizers available for ATA tuning "
			<< ataTuningName
			<< " for SonATA beams " << preludeBeamNamesStrm.str()
			<< endl;

		throw SseException(errorMsg.str(), __FILE__, __LINE__,
				SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
	}
	ChannelizerProxy *proxy = *chanList.begin();
	float64_t channelizerSkyFreqMhz = proxy->getRequestedTuneFreq();
	float64_t bandwidthMhz = (float64_t)(proxy->getOutputChannels())*(proxy->getMhzPerChannel());
	float64_t halfBandwidthMhz = proxy->getMhzPerChannel()/2.0;

		//  Get dxs for the prelude beams on this ata tuning
		DxList dxList = getNssComponentTree()->getDxsForBeams(preludeBeams);
		if (dxList.size() == 0)
		{
			stringstream errorMsg;
			errorMsg << "No dxs available for ATA tuning "
				<< ataTuningName
				<< " for SonATA beams " << preludeBeamNamesStrm.str()
				<< endl;

			throw SseException(errorMsg.str(), __FILE__, __LINE__,
					SSE_MSG_ACT_FAILED, SEVERITY_ERROR);  
		}

		// get min & max dx sky freq for dxs 
		dxMinFreqMhz = MinDxSkyFreqMhz(dxList);
		dxMaxFreqMhz = MaxDxSkyFreqMhz(dxList);
		dxMeanSkyFreqMhz = (dxMaxFreqMhz - dxMinFreqMhz)/2.0;
		dxBandwidthMhz = dxList.front()->getBandwidthInMHz();

		//if (schedulerParameters_.isAutoTuneRf())
		{
			// use the mean freq:
			double tuningFreqMhz = channelizerSkyFreqMhz;

			VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
					<< "ATA tuning: " << ataTuningName
					<< " for prelude beams: " << preludeBeamNamesStrm.str()
					<< " tuned to: " << tuningFreqMhz << " MHz "
					<< endl;);

			/*
			   It's possible for all the dxs on this tuning to be unused
			   (ie, skyfreq = -1) when not in multitarget mode, so don't 
			   report it as an error.
			   */
			if (! isMultitargetObservation() && dxMeanSkyFreqMhz < 0)
			{  
				SseArchive::SystemLog() 
					<< "Act " << getId() << ": " 
					<< "no dxs in use "
					<< "for prelude beams: " << preludeBeamNamesStrm.str()
					<< " (on ata tuning '" << ataTuningName << "') "
					<< endl;

				return;
			}

			// set tscope parameter
			if (! tscopeParameters_.setTuningSkyFreqMhz(ataTuningName, 
						tuningFreqMhz))
			{
				stringstream errorMsg;
				errorMsg << "Failure setting tuning sky freq of " 
					<< tuningFreqMhz << " MHz for ATA tuning "
					<< ataTuningName
					<< " for prelude beams " << preludeBeamNamesStrm.str()
					<< endl;

				throw SseException(errorMsg.str(), __FILE__, __LINE__,
						SSE_MSG_ACT_FAILED, SEVERITY_ERROR);    
			}

		}
		// Widen the "allowable" bandwidth by a dx's width to compensate
		// for the fact that the channelizer's "center tuning" is off center

		//double bandwidthMhz = schedulerParameters_.getBeamBandwidthMhz() 
		// + dxBandwidthMhz;

if (! isMultitargetObservation() && dxMeanSkyFreqMhz < 0)
	{  
	if (( proxy->getOutputChannels()/2)*2 == proxy->getOutputChannels() )
           {
		verifyDxMinMaxFreqsFallWithinAtaTuningBand(
				ataTuningName, dxMinFreqMhz, dxMaxFreqMhz,
				channelizerSkyFreqMhz - halfBandwidthMhz, bandwidthMhz);
            }
	else
	{
		verifyDxMinMaxFreqsFallWithinAtaTuningBand(
				ataTuningName, dxMinFreqMhz, dxMaxFreqMhz,
				channelizerSkyFreqMhz, bandwidthMhz);
	}
	}
	}
	double tuningSkyFreqMhz = tscopeParameters_.getTuningSkyFreqMhz( ataTuningName);

	if (useRf())
	{
		SseArchive::SystemLog()
			<< "Act " << getId() << ": " 
			<< "center tune freq "
			<< "for prelude beams: " << preludeBeamNamesStrm.str()
			<< " (on ata tuning '" << ataTuningName << "') "
			<< "is " << tuningSkyFreqMhz << " MHz" << endl;
		// set tscope parameter
		if (! tscopeParameters_.setTuningSkyFreqMhz(ataTuningName, 
					tuningSkyFreqMhz))
		{
			stringstream errorMsg;
			errorMsg << "Failure setting tuning sky freq of " 
				<< tuningSkyFreqMhz << " MHz for ATA tuning "
				<< ataTuningName
				<< " for prelude beams " << preludeBeamNamesStrm.str()
				<< endl;

			throw SseException(errorMsg.str(), __FILE__, __LINE__,
					SSE_MSG_ACT_FAILED, SEVERITY_ERROR);    
		}
	}

}


void ObserveActivityImp::tscopeCancelTimer(TscopeProxy* tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"tscopeCancelTimer received from " << tscopeProxy->getName() << endl;);

		tscopeReadyTimeout_.cancelTimer();
}


void ObserveActivityImp::tscopeReady(TscopeProxy* tscopeProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"tscopeReady received from " << tscopeProxy->getName() << endl;);

	if (activityWrappingUp_.get())
	{
		return;
	}

	nTscopesReady_++;

	if (nTscopesReady_ > nTscopesStarted_)
	{
		VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
				<< "Warning: " << nTscopesReady_ 
				<< " tscopeReady messages received" << endl;);
	}

		SseArchive::SystemLog() << "Act " << getId() << ": "
			<< nTscopesStarted_  
			<< " Tscopes Started " << endl;
	if (nTscopesReady_ == nTscopesStarted_)
	{

		VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
				"all Tscopes report ready" << endl;);

		tscopeReadyTimeout_.cancelTimer();

		SseArchive::SystemLog() << "Act " << getId() << ": "
			<< nTscopesReady_ 
			<< " Tscope(s) ready" << endl;

		// Save tscope status for later recording in database

		// TBD. Verify that the tscope status has correct
		// pointing info at this time.

		tscopeStatusOnTarget_ = tscopeProxy->getStatus();

		if (isPointAntsAndWaitActivity())
		{
			SseArchive::SystemLog() 
				<< "Act " << getId() << ": " 
				<< "Now on requested primary beam position."
				<< "  Waiting " << schedulerParameters_.getCommensalCalLengthMinutes()
				<< " minutes... " << endl;

			// Wait for requested amount of time
			const double SecsPerMin(60);
			int waitTimeSecs(schedulerParameters_.getCommensalCalLengthMinutes()
					* SecsPerMin);

			pointAntsAndWaitTimeout_.startTimer(
					waitTimeSecs, 
					this, &ObserveActivityImp::finishNonDxActivity,
					verboseLevel_);

		}
		else if (!useDx())
		{
			// Once tscopes are ready, activity is done.
			finishNonDxActivity();
		}
		else if (useIfc())
		{
			startIfcs(); 
		}
		else if (useTestgen())
		{
			startTestSigGens();  
		}
		else 
		{
			AssertMsg(0,"No operation specified after telescope control");
		}
	}

}

void ObserveActivityImp::setIfSource() 
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"setIfSource()" << endl;);

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Set input source..." << endl;

	// for each ifc on the list
	IfcList &ifcList = getIfcList();

	for (IfcList::iterator it = ifcList.begin(); 
			it != ifcList.end(); ++it)
	{
		IfcProxy *ifcProxy = *it;

		// set the IF source switch
		IfcParameters & ifcParams = getIfcParametersForIfcName(
				ifcProxy->getName());

		if (ifcParams.ifSourceIsTest())
		{
			ifcProxy->ifSource("test");

			VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
					"setIfSource() to test" << endl;);
		}
		else
		{
			// IF source is sky
			ifcProxy->ifSource("sky");

			VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
					"setIfSource() to sky" << endl;);
		}

		ifcProxy->requestStatusUpdate();

		// allow time for commands to be processed to avoid
		// gpib bus collisions
		sleep(1); 

	}
}

void ObserveActivityImp::startTestSigGens() 
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"startTestSigGens()" << endl;);

	if (activityWrappingUp_.get()) 
	{
		return;
	}

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Preparing test generators..." << endl;

	// watch that all the components check in
	startWatchdogTimer(tsigReadyTimeout_,
			&ObserveActivityImp::handleTestSigReadyTimeout,
			actParameters_.getComponentReadyTimeoutSecs());

	// for each TestSig on the list
	TestSigList &testSigList = getAllNeededTestSigs();

	// must set the count before starting them because components may
	// report in as ready before the loop finishes
	nTestSigsStarted_ = testSigList.size();

	for (TestSigList::iterator it = testSigList.begin(); 
			it != testSigList.end(); ++it)
	{
		TestSigProxy *testSigProxy = *it;

		if (!testSigProxy->isAlive())
		{
			const string msg("Tsig (Test Signal Gen) no longer connected");
			getObsSummaryTxtStrm() << msg <<  endl;
			terminateActivity(msg);
			return;
		}

		testSigProxy->attachObserveActivity(this);

		TestSigParameters & testSigParams = getTestSigParameters(
				testSigProxy->getName());

		// if requested, start generating the test signal
		if (testSigParams.getGenerate())
		{
			// configure the test signal generator for CW or Pulse signal
			if (testSigParams.getSignalType() == TestSigParameters::CW)
			{
				setCwTestSignal(testSigProxy, testSigParams);
			}
			else if (testSigParams.getSignalType() == TestSigParameters::PULSE)
			{
				setPulseTestSignal(testSigProxy, testSigParams);
			}
			else
			{
				cerr << "Error ObserveActivityImp::start(): unknown test generator signal type = " 
															<< testSigParams.getSignalType() << endl;
				AssertMsg(0, "Unknown test generator signal type");
			}

			testSigProxy->sigGenOn();
		}
		else
		{
			testSigProxy->quiet();
		}

		testSigProxy->requestStatusUpdate();

		// allow time for commands to be processed to avoid
		// gpib bus collisions
		sleep(1); 

		testSigProxy->requestReady();

	}

}

void ObserveActivityImp::testSigReady(TestSigProxy* testSigProxy)
{
	if (activityWrappingUp_.get())
	{
		return;
	}

	nTestSigsReady_++;
	if (nTestSigsReady_ == nTestSigsStarted_)
	{
		VERBOSE2(verboseLevel_,  "Act " << getId() << ": " << 
				"all TestSigs (test signal gens) report ready" << endl;);

		tsigReadyTimeout_.cancelTimer();

		SseArchive::SystemLog()  << "Act " << getId() << ": "
			<< nTestSigsReady_ << " TestSig(s) ready"
			<< endl;

		startDxs();

	}

}




IfcParameters & ObserveActivityImp::getIfcParametersForIfcName(const string &ifcName)
{
	if (ifcName == Ifc1Name)
	{
		return ifc1Parameters_;
	}
	else if (ifcName == Ifc2Name)
	{
		return ifc2Parameters_;
	}
	else if (ifcName == Ifc3Name)
	{
		return ifc3Parameters_;
	}
	else 
	{
		stringstream strm;
		strm << "unknown ifc, can't get parameters for: " << ifcName << endl;
		throw SseException(strm.str(), __FILE__, __LINE__,
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

}



void ObserveActivityImp::verifyDxMinMaxFreqsFallWithinAtaTuningBand(
		const string &ataTuningName, double dxMinFreqMhz, double dxMaxFreqMhz,
		double tuningCenterMhz, double tuningBandwidthMhz)
{
	if (fabs(dxMinFreqMhz - tuningCenterMhz) > tuningBandwidthMhz / 2)
	{
		stringstream strm;
		strm.precision(1); // show N places after the decimal
		strm.setf(std::ios::fixed);  // show all decimal places up to precision

		strm << "Dx minimum skyfreq of " << dxMinFreqMhz << " MHz"
			<< " is outside the " << tuningBandwidthMhz << " MHz"
			<< " wide beam bandwidth on ata tuning '" << ataTuningName
			<< "' centered at " << tuningCenterMhz << " MHz" << endl;

		throw SseException(strm.str(), __FILE__, __LINE__,
				SSE_MSG_OUT_OF_BAND_FREQ, SEVERITY_ERROR); 

	}
	else if (fabs(dxMaxFreqMhz - tuningCenterMhz) > tuningBandwidthMhz / 2)
	{
		stringstream strm;
		strm.precision(1); // show N places after the decimal
		strm.setf(std::ios::fixed);  // show all decimal places up to precision

		strm << "Dx maximum skyfreq of " << dxMaxFreqMhz << " MHz"
			<< " is outside the " << tuningBandwidthMhz << " MHz"
			<< " wide beam bandwidth on ata tuning '" << ataTuningName 
			<< "' centered at " 
			<< tuningCenterMhz << " MHz" << endl;

		throw SseException(strm.str(),  __FILE__, __LINE__,
				SSE_MSG_OUT_OF_BAND_FREQ, SEVERITY_ERROR); 

	}
}

void ObserveActivityImp::setIfcAttenuators(IfcParameters &ifcParams,
		double ifcSkyFreqMhz,
		IfcProxy *ifcProxy)
{
	// Set the attenuators, either automatically or manually.
	// TBD can the manual call be moved into an 'else' clause below,
	// or must the attenuators be set to the 'manual' values
	// first before trying to set them automatically?

	int attnLeft = ifcParams.getAttnl();
	int attnRight = ifcParams.getAttnr();

	if (useRf())
	{
		// If using the RF, then adjust the attenuators based on the
		// skyFreq the Ifc is tuned to

		int offsetDbLeft = ifAttnDbOffsetTable_->getDbOffsetLeft(ifcSkyFreqMhz);
		attnLeft += offsetDbLeft;

		int offsetDbRight = ifAttnDbOffsetTable_->getDbOffsetRight(ifcSkyFreqMhz);
		attnRight += offsetDbRight;

	/*	SseArchive::SystemLog() 
			<< "Act " << getId() << ": " << ifcProxy->getName() 
			<< " attn settings (dB), adjusted for sky freq: " 
			<< ifcSkyFreqMhz << " MHz" << endl 
			<< "L: " << attnLeft << " dB" 
			<< "  (includes "
			<< offsetDbLeft << " dB freq-dependent offset)" << endl
			<< "R: " << attnRight << " dB" 
			<< "  (includes "
			<< offsetDbRight << " dB freq-dependent offset)" << endl
			<< endl;
	*/
	}
	else
	{
		SseArchive::SystemLog() 
			<< "Act " << getId() << ": " << ifcProxy->getName() 
			<< " attenuator settings (dB):  "  
				<< "L: " << attnLeft << " dB" << "   R: " << attnRight << " dB"
					<< endl;
	}

	ifcProxy->attn(attnLeft, attnRight);

	if (ifcParams.useAutoAttnCtrl())
	{
		SseArchive::SystemLog() 
			<< "Act " << getId() << ": "
			<< ifcProxy->getName() << " making automatic attenuator control changes..." 
			<< endl;

		ifcProxy->stxSetVariance(ifcParams.getVarLeft(),
				ifcParams.getVarRight(),
				ifcParams.getStxTolerance(),
				ifcParams.getStxLength());
	}

}

static string getDxNames(DxList & dxList)
{
	stringstream strm;
	for (DxList::iterator it=dxList.begin(); it != dxList.end(); ++it)
	{
		strm << (*it)->getName() << " ";
	}
	return strm.str();
}

void ObserveActivityImp::startIfcs()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"startIfcs()" << endl;);

	if (activityWrappingUp_.get())
	{
		return;
	}

	//SseArchive::SystemLog() << "Act " << getId() << ": "
	//	<< "Preparing Ifcs..." << endl;

	// Start the watchdog timer before starting the components to avoid
	// a potential race condition with the incoming "component ready" messages.

	startWatchdogTimer(ifcReadyTimeout_, 
			&ObserveActivityImp::handleIfcReadyTimeout,
			actParameters_.getComponentReadyTimeoutSecs());

	try
	{
		IfcList &ifcList = getIfcList();

		// must set the started counter before actually starting them because
		// components may report in as ready before the loop finishes

		nIfcsStarted_ = ifcList.size();

		for (IfcList::iterator ifcIt = ifcList.begin(); 
				ifcIt != ifcList.end(); ++ifcIt)
		{
			IfcProxy *ifcProxy = *ifcIt;
			if (! ifcProxy->isAlive())
			{
				throw SseException("IFC no longer connected\n",
						__FILE__,  __LINE__, 
						SSE_MSG_MISSING_IFC, SEVERITY_ERROR);
			}

			// Only look at the dxs for the beams on this ifc that have 
			// been requested for use.
			// First get all the beam names for this ifc:
			vector<string> beamNamesForIfc(
					expectedTree_->getBeamsForIfc(ifcProxy->getName()));
			Assert(beamNamesForIfc.size() > 0);

			// Get all the beams requested overall
			vector<string> allBeamsToUse(schedulerParameters_.getBeamsToUse());
			Assert(allBeamsToUse.size() > 0);

			// Find the intersection
			sort(beamNamesForIfc.begin(), beamNamesForIfc.end());
			sort(allBeamsToUse.begin(), allBeamsToUse.end());
			vector<string> requestedBeamNamesForIfc;
			set_intersection(beamNamesForIfc.begin(), beamNamesForIfc.end(),
					allBeamsToUse.begin(), allBeamsToUse.end(),
					back_inserter(requestedBeamNamesForIfc));
			Assert(requestedBeamNamesForIfc.size() > 0);

			DxList dxList =
				getNssComponentTree()->getDxsForBeams(requestedBeamNamesForIfc);

			// get min & max dx sky freq for dxs on this ifc
			double dxMinFreqMhz(MinDxSkyFreqMhz(dxList));

			double dxMaxFreqMhz(MaxDxSkyFreqMhz(dxList));

			// Skip this ifc if no dxs are available for it,
			// i.e., either none are attached, or all are marked as
			// "don't use (ie, skyfreq < 0).

			if (dxList.size() == 0)
			{
				SseArchive::SystemLog() 
					<< "Act " << getId() << ": "
					<< "No dxs available for "
					<< ifcProxy->getName() << endl;

				ifcReady(ifcProxy);
				continue;
			} 
			else if (dxMinFreqMhz < 0.0)
			{
				SseArchive::SystemLog()
					<< "Act " << getId() << ": "
					<< ifcProxy->getName() << ": "
					<< "dx skyfreq is < 0 for all dxs on this ifc"
					<< " (i.e., that marks them as 'not to be used') " << endl;

				ifcReady(ifcProxy);
				continue;
			}

			// debug - print the dxs on this ifc
			VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
					<< "dxs for " << ifcProxy->getName() << ": " 
					<< getDxNames(dxList) << endl;);

			IfcParameters & ifcParams = getIfcParametersForIfcName(
					ifcProxy->getName());

			ifcProxy->attachObserveActivity(this);

			/*
			   Use the average of all the dx freqs on this ifc to
			   determine the freq to use for the attn lookup.
			   This isn't correct if the ifc really has different beams
			   on each pol and those beams are using widely separated
			   frequency ranges.  But to do this properly we need
			   to know which beam is on which pol, which is not information
			   that is currently available.  Also, in multitarget mode,
			   all the beams should have the same dx tunings, so
			   it won't matter.
TBD: review this.
*/
			double ifcSkyFreqMhz = (dxMinFreqMhz + dxMaxFreqMhz)/2.0;

			setIfcAttenuators(ifcParams, ifcSkyFreqMhz, ifcProxy);

			// allow time for commands to be processed to avoid
			// gpib bus collisions
			sleep(2); 

			ifcProxy->requestStatusUpdate();

			ifcProxy->requestReady();
		} 

	}
	catch (SseException &except)
	{
		stringstream strm;

		strm << "startIfcs() failed: " << except.descrip();
		SseMessage::log(MsgSender, getId(),
				except.code(), except.severity(), strm.str(),
				except.sourceFilename(), except.lineNumber());

		terminateActivity(strm.str());
		return;  
	}
	catch (...)
	{
		stringstream strm;
		strm << "startIfcs() failed for unknown reason "  << endl;
		terminateActivity(strm.str());
		return;
	}

}



bool ObserveActivityImp::validIfcStatus(IfcProxy* ifcProxy)
{
	bool valid(true);
	SseMsgCode msgCode(SSE_MSG_UNINIT);
	NssMessageSeverity severityLevel(SEVERITY_INFO);
	bool warn(false);
	stringstream strm;

	Polarization ifcPol(ifcProxy->getStxPol());

	bool useLeft(ifcPol == POL_LEFTCIRCULAR ||
			ifcPol == POL_BOTH);

	bool useRight(ifcPol == POL_RIGHTCIRCULAR ||
			ifcPol == POL_BOTH);

	if (! (useLeft || useRight))
	{
		strm << "Invalid stx polarity in ifc status" << endl;
		severityLevel = SEVERITY_ERROR;
		msgCode = SSE_MSG_STX_INVALID_STAT;
		valid = false;
	}
	else if (! ifcProxy->goodStxStatus())
	{
		strm << "Invalid stx status: " 
			<< ifcProxy->getStxStatusString() << endl;

		severityLevel = SEVERITY_ERROR;
		msgCode = SSE_MSG_STX_INVALID_STAT;
		valid = false;
	}
	else
	{
		double leftVar = ifcProxy->getStatus().stxVarLeft;
		double rightVar = ifcProxy->getStatus().stxVarRight;

		// check that variances are in error range.
		if (actParameters_.checkVarianceErrorLimits())
		{
			// Lower error limits, each pol
			if (useLeft && (leftVar < actParameters_.varianceErrorLowerLimit()))
			{
				strm << "Stx LCP variance of " << leftVar 
					<< " is below lower error limit of " 
					<< actParameters_.varianceErrorLowerLimit() << endl;
				valid = false;
				severityLevel = SEVERITY_ERROR;
				msgCode = SSE_MSG_STX_LVAR_ELW;
			}
			else if (useRight && (rightVar < actParameters_.varianceErrorLowerLimit()))
			{
				strm << "Stx RCP variance of " << rightVar 
					<< " is below lower error limit of " 
					<< actParameters_.varianceErrorLowerLimit() << endl;
				valid = false;
				severityLevel = SEVERITY_ERROR;
				msgCode = SSE_MSG_STX_RVAR_ELW;
			}

			// upper error limits, each pol

			if (useLeft && (leftVar > actParameters_.varianceErrorUpperLimit()))
			{
				strm << "Stx LCP variance of " << leftVar 
					<< " is above upper error limit of " 
					<< actParameters_.varianceErrorUpperLimit() << endl;
				valid = false;
				severityLevel = SEVERITY_ERROR;
				msgCode = SSE_MSG_STX_LVAR_EUP;
			}
			else if (useRight && (rightVar > actParameters_.varianceErrorUpperLimit()))
			{
				strm << "Stx RCP variance of " << rightVar 
					<< " is above upper error limit of " 
					<< actParameters_.varianceErrorUpperLimit() << endl;
				valid = false;
				severityLevel = SEVERITY_ERROR;
				msgCode = SSE_MSG_STX_RVAR_EUP;
			}
		}

		if (valid && actParameters_.checkVarianceWarnLimits())
		{
			// Lower warning limits, each pol

			if (useLeft && (leftVar < actParameters_.varianceWarnLowerLimit()))
			{
				strm << "Stx LCP variance of " << leftVar 
					<< " is below lower warning limit of " 
					<< actParameters_.varianceWarnLowerLimit() << endl;
				warn = true;
				severityLevel = SEVERITY_WARNING;
				msgCode = SSE_MSG_STX_LVAR_WLW;

			}
			else if (useRight && (rightVar < actParameters_.varianceWarnLowerLimit()))
			{
				strm << "Stx RCP variance of " << rightVar 
					<< " is below lower warning limit of " 
					<< actParameters_.varianceWarnLowerLimit() << endl;
				warn = true;
				severityLevel = SEVERITY_WARNING;
				msgCode = SSE_MSG_STX_RVAR_WLW;
			}

			// uppper warning limits, each pol

			if (useLeft && (leftVar > actParameters_.varianceWarnUpperLimit()))
			{
				strm << "Stx LCP variance of " << leftVar 
					<< " is above upper warning limit of " 
					<< actParameters_.varianceWarnUpperLimit() << endl;
				warn = true;
				severityLevel = SEVERITY_WARNING;
				msgCode = SSE_MSG_STX_LVAR_WUP;
			}
			else if (useRight && (rightVar > actParameters_.varianceWarnUpperLimit()))
			{
				strm << "Stx RCP variance of " << rightVar 
					<< " is above upper warning limit of " 
					<< actParameters_.varianceWarnUpperLimit() << endl;
				warn = true;
				severityLevel = SEVERITY_WARNING;
				msgCode = SSE_MSG_STX_RVAR_WUP;
			}

		}
	}

	if (!valid || warn)
	{
		SseMessage::log(ifcProxy->getName(), getId(),
				msgCode, severityLevel, 
				strm.str(), __FILE__, __LINE__);
	}

	return valid;
}

void ObserveActivityImp::ifcReady(IfcProxy* ifcProxy)
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": " 
			<< "ifc " << ifcProxy->getName()
			<< " reported in  ready" << endl;);

	try {

		if (activityWrappingUp_.get())
		{
			return;
		}

		if (dbParameters_.useDb())
		{
			updateIfcStatus(ifcProxy->getStatus());
		}

		// get the list of dxs attached to this ifc
		DxList & dxList = getNssComponentTree()->getDxsForIfc(ifcProxy);
		double dxMinFreqMhz(MinDxSkyFreqMhz(dxList));

		// only check status if this ifc is being used (ie, has some active dxs)
		if (dxList.size() > 0 && dxMinFreqMhz > 0.0)
		{
			if (! validIfcStatus(ifcProxy))
			{
				terminateActivity(ifcProxy->getName() + ": invalid ifc state");
				return;
			}
		}

		nIfcsReady_++;
		if (nIfcsReady_ == nIfcsStarted_)
		{
			VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
					"all Ifcs report ready" << endl;);

			ifcReadyTimeout_.cancelTimer();

			//SseArchive::SystemLog() << "Act " << getId() << ": "
			//	<< nIfcsReady_ << " Ifc(s) ready" << endl;

			if (useTestgen())
			{
				startTestSigGens(); 
			}
			else
			{
				startDxs();
			}
		}

	}
	catch (SseException &except) 
	{
		SseMessage::log(
				MsgSender, getId(),
				except.code(), except.severity(), except.descrip(), 
				except.sourceFilename(), except.lineNumber());

		terminateActivity(except.descrip());
		return;
	}

}

void ObserveActivityImp::startDxs()
{
	VERBOSE2(verboseLevel_, "Act " << getId() << ": startDxs()" << endl;);

	if (activityWrappingUp_.get()) 
	{
		return;
	}

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Preparing dxs..." << endl;

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"going though dx list, creating activity units..." << endl;);

	ActUnitList actUnitList;
	int actUnitId = 1;

	// for each dx, start an activityUnit
	DxList &dxList = getDxList();
	DxList::iterator p;
	for (p = dxList.begin(); p != dxList.end(); ++p)
	{
		VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
				"ObsAct:startup:proxy addr: " << *p << endl;);
		DxProxy *proxy = *p;

		// make sure dx is still connected.
		// also make sure it is to be used (skyfreq > 0)

		if (! proxy->isAlive())
		{
			// Dx is disconnected

			stringstream strm;
			strm << "ObserveActivityImp::processDxs(): "
				<< "Can't start activityUnit for "
					<< "disconnected Dx " << proxy->getName()
					   << endl;
			SseMessage::log(proxy->getName(),
					getId(), SSE_MSG_DX_DISCONNECT,
					SEVERITY_ERROR, strm.str(),
					__FILE__, __LINE__);

			getObsSummaryTxtStrm() << "Can't start activityUnit for "
				<< "disconnected Dx " << proxy->getName()
				<< endl;

		}
		else if (proxy->getDxSkyFreq() > 0.0)  
		{
			createActivityUnit(proxy, actUnitId, actUnitList);
			actUnitId++;
		}
	}

	getObsSummaryTxtStrm() << endl;

	if (actUnitList.size() > 0)
	{
		actUnitCreatedList_ = actUnitList;
		actUnitStillWorkingListMutexWrapper_ = actUnitList;

		prepareActivityUnits(actUnitList);
	} 
	else
	{
		const string msg("No dxs ready for use");
		getObsSummaryTxtStrm() << msg <<  endl;
		terminateActivity(msg);
		return;
	}
}

// Create an activity Unit for the DxProxy
// and put it into the actUnitList
void ObserveActivityImp::createActivityUnit(DxProxy *proxy,
		int actUnitId,
		ActUnitList &actUnitList)
{

#ifdef Prelude
	// TBD: This is obsolete in Sonata.  Delete it eventually.

	// Make the assigned bandwidth the same as
	// the intrinsic bandwidth, unless it's been
	// set previously with the manual override.

	if (! useManuallyAssignedDxBandwidth())
	{
		dxActParameters_.assignedBandwidth =
			proxy->getIntrinsics().maxSubchannels;
	}
#endif

	dxActParameters_.dxSkyFreq = proxy->getDxSkyFreq();

	dxActParameters_.channelNumber = proxy->getChannelNumber();

	try 
	{
		if (! useIfc())
		{
			// Dx test only
			dxActParameters_.ifcSkyFreq = proxy->getDxSkyFreq();
		}
		else
		{
			string beamName = expectedTree_->getBeamForDx(proxy->getName());
			if (beamName == "")
			{
				throw SseException("failed to find prelude beam name for dx",
						__FILE__, __LINE__);
			}

			// determine ata tuning freq associated with this dx's beam   
			string ataTuningName = getTuningNameForPreludeBeam(beamName);
			if (ataTuningName == "")
			{
				throw SseException("failed to get ATA tuning name for prelude beam",
						__FILE__, __LINE__);
			}

			double tuningSkyFreqMhz = tscopeParameters_.getTuningSkyFreqMhz(
					SseUtil::strToLower(ataTuningName));

			/* ifc doesn't really have a tunable sky freq anymore, but the
			   dx still needs to be able to know how to offset tune from the
			   center of the beamformer input band.
			   */
			dxActParameters_.ifcSkyFreq = tuningSkyFreqMhz +
				tscopeParameters_.getBasebandCenterTuneOffsetMhz();

		}

		/*
		   Make the rcvrSkyFreq the same as the ifcSkyFreq so that 
		   rf birdie masks use the correct offset.
		   */
		dxActParameters_.rcvrSkyFreq = dxActParameters_.ifcSkyFreq;

		VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
				"setting sky freqs for dx: " << proxy->getName()
				<< " dxSkyFreq=" <<  dxActParameters_.dxSkyFreq
				<< " ifcSkyFreq= " <<  dxActParameters_.ifcSkyFreq
				<< " rcvrSkyFreq= " <<  dxActParameters_.rcvrSkyFreq
				<< " MHz" << endl;);

		// log dx sky freq in observation summary
		getObsSummaryTxtStrm() << "Dx: " 
			<< proxy->getName()
			<< " dxSkyFreq=" <<  dxActParameters_.dxSkyFreq
			<< " ifcSkyFreq=" <<  dxActParameters_.ifcSkyFreq
			<< " rcvrSkyFreq=" <<  dxActParameters_.rcvrSkyFreq
			<< " MHz" << endl;

		ActivityUnit *actUnit = new ActivityUnitImp(
				this, actUnitId, proxy, dxActParameters_, verboseLevel_);

		actUnitList.push_back(actUnit);
	} 
	catch (const SseException & except)
	{
		stringstream strm;
		strm << "act unit create failed for dx " 
			<<  proxy->getName() << ": " 
			<< except.sourceFilename() << "[" << except.lineNumber() << "]"
			<< " " << except.descrip() << endl;

		SseMessage::log(MsgSender, getId(),
				except.code(), except.severity(),
				strm.str(), __FILE__, __LINE__);
	}
	catch (...)
	{
		stringstream strm;
		strm << "act unit create failed, caught unexpected exception" 
			<< " for dx " << proxy->getName() << endl;
		SseArchive::ErrorLog() << strm.str();

	}
}


void ObserveActivityImp::prepareActivityUnits(const ActUnitList &actUnitList)
{
	// go through the list of activity units & initialize them

	// Must set the total number of actunits counter
	// before any actunits are initialized
	// because the units may report back as ready before
	// the loop finishes.

	nActUnitsStarted_ = actUnitList.size();
	Assert(nActUnitsStarted_ > 0);

	nActUnitsStillWorking_ = nActUnitsStarted_;

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"starting N act units: " << nActUnitsStarted_ << endl;);

	// start a watchdog timer, waiting for all activity units to
	// report in as ready.
	startWatchdogTimer(dxTunedTimeout_,
			&ObserveActivityImp::handleDxTunedTimeout,
			actParameters_.getComponentReadyTimeoutSecs());

	stringstream dxNames;

	ActUnitList::const_iterator it;
	for (it = actUnitList.begin(); it != actUnitList.end(); ++it)
	{
		ActivityUnit *actUnit = *it;
		dxNames << actUnit->getDxName() << " ";

		actUnit->initialize();
	}

	obsSummaryTxtStrm_ << "Number of Activity Units Initialized: " <<
		nActUnitsStarted_ << endl;


	SseArchive::SystemLog()  << "Act " << getId() << ": "
		<< nActUnitsStarted_ 
		<< " Activity Unit(s) (dxs) initialized " << endl;

	SseArchive::SystemLog()  << "Act " << getId() << ": "
		<< "Dxs in use: " << dxNames.str() << endl;

	if (actParameters_.compareDxDataProducts())
	{

		SseArchive::SystemLog() << "Act " << getId() << ": "
			<< "Dx comparison mode is on." << endl;

		// make sure there were at least 2 activity units in this activity
		if (nActUnitsStarted_ < 2)
		{
			SseArchive::SystemLog() << "Act " << getId() << ": "
				<< "Warning: Can't compare dxs since only 1 Dx"
				<< " (or Dx Pair) is being used." << endl;
		}
	}


	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Dx Min/Max Freq: " << getMinDxSkyFreqMhz()
		<< " / " << getMaxDxSkyFreqMhz() 
		<< " MHz " << endl;

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Obs length: " 
		<< dataCollectionLengthSecs_ << " secs" << endl;

	SseArchive::SystemLog() << "Act " << getId() << ": "
		<< "Waiting for dxs to report in as 'tuned'..." 
		<< endl;

	// note that the activity units will all report back in as
	// ready (ie., when their dxs report in as tuned)
	// and then we can proceed to send the start time.

}



// adjust the dx activity parameters:
// - set the activity ID
// - load the operations bitset
//
void ObserveActivityImp::adjustDxActivityParameters(DxActivityParameters &ap,
		const DxOpsBitset &dxOpsBitset)
{
	ap.activityId = getId();
	DxOpsBitset adjustedDxOpsBitset = dxOpsBitset;

	// if requested, disable pulse detection
	if (! dxParameters_.allowPulseDetection())
	{
		adjustedDxOpsBitset.reset(PULSE_DETECTION);
	}

	// if requested, disable use of the recent RFI mask
	if (! dxParameters_.recentRfiEnable())
	{
		adjustedDxOpsBitset.reset(APPLY_RECENT_RFI_MASK);
	}

	// Turn on freqency inversion, if appropriate for
	// the parts of the signal path being used.

	ActParameters::FreqInvertOption option = actParameters_.getFreqInvertOption();
	if ((option == ActParameters::FREQ_INVERT_ALWAYS)
			|| (useRf() && option == ActParameters::FREQ_INVERT_RF)
			|| (useIfc() && option == ActParameters::FREQ_INVERT_IF)) 
	{
		adjustedDxOpsBitset.set(FREQ_INVERSION);
	}

	if (isMultitargetObservation() || forceArchivingAroundCenterTuning())
	{
		adjustedDxOpsBitset.set(PROCESS_SECONDARY_CANDIDATES);
	}

	// copy the dx operations bitset value into the operations word
	ap.operations =  static_cast<uint32_t>(adjustedDxOpsBitset.to_ulong());

}


void ObserveActivityImp::turnOffTscopes()
{

	TscopeList &tscopeList = getAllNeededTscopes();
	for (TscopeList::iterator tscopeProxy = tscopeList.begin(); 
			tscopeProxy != tscopeList.end(); tscopeProxy++)
	{

		// TBD.  Any action needed here?

	}
}

void ObserveActivityImp::turnOffTestSigs()
{

	TestSigList::iterator tsig;
	TestSigList &testSigList = getAllNeededTestSigs();
	for (tsig = testSigList.begin(); tsig != testSigList.end(); tsig++) 
	{
		VERBOSE1(verboseLevel_,  "Act " << getId() << ": " <<
				"TSIG off." << endl;);
		(*tsig)->sigGenOff();

		(*tsig)->requestStatusUpdate();
	}

}

void ObserveActivityImp::turnOffIfcs()
{
	IfcList::iterator ifc;
	IfcList &ifcList = getIfcList();
	for (ifc = ifcList.begin(); ifc != ifcList.end(); ifc++)
	{
		VERBOSE1(verboseLevel_,  "Act " << getId() << ": " <<
				"IFC off." << endl;);
		// TBD (*ifc)->off();
	}

}

void ObserveActivityImp::setCwTestSignal(TestSigProxy* testSigProxy, 
		const TestSigParameters &testSigParameters)
{

	VERBOSE1(verboseLevel_,  "Act " << getId() << ": " <<
			"setCwTestSignal()." << endl;);

	DriftingTone dtone;

	dtone.start.frequency = testSigParameters.getFrequency();
	dtone.start.freqUnits = Tone::MHz;
	dtone.start.amplitude = testSigParameters.getCwAmplitude();
	dtone.start.ampUnits = Tone::dBm;

	dtone.driftRate = testSigParameters.getDriftRate();
	dtone.duration = testSigParameters.getSweepTime();

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " <<
			"\tDrifting Tone \n" << dtone << endl;);

	testSigProxy->cwTest(dtone);

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " <<
			testSigProxy->getStatus() << endl;);

}

void ObserveActivityImp::setPulseTestSignal(TestSigProxy* testSigProxy,
		TestSigParameters &testSigParameters)
{
	VERBOSE1(verboseLevel_, "Act " << getId() << ": " <<
			"setPulseTestSignal()." << endl;);

	DriftingTone dtone;

	dtone.start.frequency = testSigParameters.getFrequency();
	dtone.start.freqUnits = Tone::MHz;
	dtone.start.amplitude = testSigParameters.getCwAmplitude();
	dtone.start.ampUnits = Tone::dBm;

	dtone.driftRate = testSigParameters.getDriftRate();
	dtone.duration = testSigParameters.getSweepTime();

	PulseSigParams pulse;

	pulse.amplitude = testSigParameters.getPulseAmplitude();
	pulse.period = testSigParameters.getPulsePeriod();
	pulse.duration = testSigParameters.getPulseWidth();

	PulsedTone ptone;
	ptone.pulse = pulse;
	ptone.driftTone = dtone;

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " <<
			"\tPulsed Tone: " << ptone << endl;);

	testSigProxy->pulseTest(ptone);

	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " <<
			testSigProxy->getStatus() << endl;);

}

void ObserveActivityImp::allDataCollectionCompleteWrapup()
{
	VERBOSE2(verboseLevel_,  "Act " << getId() << ": " <<
			"allDataCollectionCompleteWrapup" << endl
			<< "Turning off TestSigs, ifcs" << endl);

	if (useTscope())
	{
		turnOffTscopes();
	}

	if (useTestgen())
	{
		// Disable this.
		// Let test signal run until either restarted
		// or turned off by the next activity.
		// This allows for easier followup testing.

		//  turnOffTestSigs();
	}

	if (useIfc())
	{
		// TBD turnOffIfcs();
	}

	detachAllNonDxComponents();
}

void ObserveActivityImp::detachAllNonDxComponents()
{
	// Make sure this method only executes once, so that
	// this activity doesn't interfere with any other activities
	// that may be using the components.

	if (allNonDxComponentsAreDetached_)
	{
		return;
	}

	VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
			"detachAllNonDxComponents" << endl);

	allNonDxComponentsAreDetached_ = true;

	if (useTscope())
	{
		TscopeList &tscopeList = getAllNeededTscopes();
		for (TscopeList::iterator tscopeProxy = tscopeList.begin(); 
				tscopeProxy != tscopeList.end(); tscopeProxy++)
		{
			(*tscopeProxy)->detachObserveActivity();
		}
	}

	if (useTestgen())
	{
		TestSigList::iterator tsig;
		TestSigList &testSigList = getAllNeededTestSigs();
		for (tsig = testSigList.begin(); tsig != testSigList.end(); tsig++)
		{
			(*tsig)->detachObserveActivity();
		}
	}

	if (useIfc())
	{
		IfcList::iterator ifc;
		IfcList &ifcList = getIfcList();
		for (ifc = ifcList.begin(); ifc != ifcList.end(); ifc++)
		{
			(*ifc)->detachObserveActivity();
		}
	}
}

// ---- utilities -----

void ObserveActivityImp::determineDesiredOperations()
{
	// Make sure the operations bits are set as needed to follow the
	// method call chains all the way down.  I.e., if you set RF_TUNE
	// USE_IFC must also be set.  TBD revisit this to make simpler
	// and allow more combinations?

	ObserveActivityOpsBitset & actOpsBitset(getActOpsBitset());


	// If RF_TUNE is set, then force USE_IFC to be set
	if (actOpsBitset.test(RF_TUNE))
	{
		actOpsBitset.set(USE_IFC);
	}


}

void ObserveActivityImp::setDiskStatusMsg(const string &msg)
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(diskStatusMsgMutex_);

	diskStatusMsg_ = msg;
}

string ObserveActivityImp::getDiskStatusMsg() const
{
	ACE_Guard<ACE_Recursive_Thread_Mutex> guard(diskStatusMsgMutex_);

	return diskStatusMsg_;
}


// convenience methods for accessing operations bits:

bool ObserveActivityImp::observingTargets()
{
	return getActOpsBitset().test(POINT_AT_TARGETS);
}

bool ObserveActivityImp::useRf()
{
	return getActOpsBitset().test(RF_TUNE);
}

bool ObserveActivityImp::useTscope()
{
	return getActOpsBitset().test(USE_TSCOPE);
}

bool ObserveActivityImp::useIfc()
{
	return getActOpsBitset().test(USE_IFC);
}

bool ObserveActivityImp::useDx()
{
	return getActOpsBitset().test(USE_DX);
}


bool ObserveActivityImp::useTestgen()
{
	// for each TestSig on the list
	TestSigList &testSigList = getAllNeededTestSigs();

	for (TestSigList::iterator it = testSigList.begin(); 
			it != testSigList.end(); ++it)
	{
		TestSigProxy *testSigProxy = *it;

		TestSigParameters & testSigParams = getTestSigParameters(
				testSigProxy->getName());

		// set the IF source switch
		if (testSigParams.getEnable())
			return (getActOpsBitset().test(TEST_SIGNAL_GEN)   // Act wants it
					&& testSigParams.getEnable());  // user has enabled it
	}
	return ( false );
}

bool ObserveActivityImp::followUpObservationEnabled()
{
	return getActOpsBitset().test(FOLLOW_UP_OBSERVATION);
}

bool ObserveActivityImp::isOffObservation()
{
	return getActOpsBitset().test(OFF_OBSERVATION);
}

bool ObserveActivityImp::isGridWestObservation()
{
	return getActOpsBitset().test(GRID_WEST_OBSERVATION);
}

bool ObserveActivityImp::isGridSouthObservation()
{
	return getActOpsBitset().test(GRID_SOUTH_OBSERVATION);
}

bool ObserveActivityImp::isGridOnObservation()
{
	return getActOpsBitset().test(GRID_ON_OBSERVATION);
}

bool ObserveActivityImp::isGridNorthObservation()
{
	return getActOpsBitset().test(GRID_NORTH_OBSERVATION);
}

bool ObserveActivityImp::isGridEastObservation()
{
	return getActOpsBitset().test(GRID_EAST_OBSERVATION);
}

bool ObserveActivityImp::isCalObservation()
{
	return getActOpsBitset().test(CALIBRATE);
}

bool ObserveActivityImp::isAutoselectAntsActivity()
{
	return getActOpsBitset().test(AUTOSELECT_ANTS);
}

//JR - Added for Tscope setup
bool ObserveActivityImp::isTscopeSetupActivity()
{
	return getActOpsBitset().test(TSCOPE_SETUP);
}

bool ObserveActivityImp::isPrepareAntsActivity()
{
	return getActOpsBitset().test(PREPARE_ANTS);
}

bool ObserveActivityImp::isFreeAntsActivity()
{
	return getActOpsBitset().test(FREE_ANTS);
}

bool ObserveActivityImp::isBeamformerResetActivity()
{
	return getActOpsBitset().test(BEAMFORMER_RESET);
}

bool ObserveActivityImp::isBeamformerInitActivity()
{
	return getActOpsBitset().test(BEAMFORMER_INIT);
}

bool ObserveActivityImp::isBeamformerAutoattenActivity()
{
	return getActOpsBitset().test(BEAMFORMER_AUTOATTEN);
}

bool ObserveActivityImp::isPointAntsAndWaitActivity()
{
	return getActOpsBitset().test(POINT_ANTS_AND_WAIT);
}

bool ObserveActivityImp::isOnObservation()
{
	return getActOpsBitset().test(ON_OBSERVATION);
}

bool ObserveActivityImp::createRecentRfiMaskEnabled()
{
	return dxParameters_.recentRfiEnable() &&
		getActOpsBitset().test(CREATE_RECENT_RFI_MASK);
}

bool ObserveActivityImp::classifyAllSignalsAsRfiScan()
{
	return getActOpsBitset().test(CLASSIFY_ALL_SIGNALS_AS_RFI_SCAN);
}

bool ObserveActivityImp::isMultitargetObservation()
{
	return getActOpsBitset().test(MULTITARGET_OBSERVATION);
}

bool ObserveActivityImp::forceArchivingAroundCenterTuning()
{
	return getActOpsBitset().test(FORCE_ARCHIVING_AROUND_CENTER_TUNING);
}

bool ObserveActivityImp::doNotReportConfirmedCandidatesToScheduler()
{
	return getActOpsBitset().test(DO_NOT_REPORT_CONFIRMED_CANDIDATES_TO_SCHEDULER);
}

//------------- get needed components -------

TscopeList & ObserveActivityImp::getAllNeededTscopes()
{
	return tscopeList_;
}

TestSigList & ObserveActivityImp::getAllNeededTestSigs()
{
	return getNssComponentTree()->getTestSigs();
}

IfcList & ObserveActivityImp::getIfcList()
{
	return ifcList_;
}

DxList & ObserveActivityImp::getDxList()
{
	return dxList_;
}


void ObserveActivityImp::handleTscopeReadyTimeout()
{
	terminateActivity("timed out waiting for tscope ready");
}

void ObserveActivityImp::handleTestSigReadyTimeout()
{
	terminateActivity("timed out waiting for test sig ready");
}


void ObserveActivityImp::handleIfcReadyTimeout()
{
	terminateActivity("timed out waiting for ifc ready");
}

void ObserveActivityImp::startWatchdogTimer(
		Timeout<ObserveActivityImp> & timeout,
		void (ObserveActivityImp::*methodToCallOnTimeout)(),
		int waitDurationSecs)
{
	if (getActParameters().useWatchdogTimers())
	{
		timeout.startTimer(waitDurationSecs,
				this,
				methodToCallOnTimeout,
				verboseLevel_);
	} 
	else
	{
		VERBOSE2(verboseLevel_, "Act " << getId() << ": " <<
				"watchdog timer disabled for "
				<< timeout.getName() << endl;);
	}
}

// ---------------------------------------
// ----- database methods 

void ObserveActivityImp::checkForZeroActId()
{
	if (!getId())
	{
		throw SseException("Uninitialized DB Activity ID\n",
				__FILE__, __LINE__, SSE_MSG_DBERR, SEVERITY_ERROR);
	}

}

void ObserveActivityImp::storeParametersInDb()
{
	if (dbParameters_.useDb())
	{
		try
		{
			recordParameters();
		}
		catch (SseException &except)
		{
			SseMessage::log(MsgSender, getId(),
					except.code(), except.severity(), except.descrip(),
					except.sourceFilename(), except.lineNumber());

			terminateActivity(except.descrip());
			return;
		}
	}
}

void ObserveActivityImp::recordParameters()
{
	const string methodName("recordParameters");

	if (!dbParameters_.getDb())
	{
		throw SseException(
				methodName + " MySQL error: NULL db\n",
				__FILE__, __LINE__, SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	unsigned int actParametersId = 
		actParameters_.record(dbParameters_.getDb());

	unsigned int ifc1ParametersId = 
		ifc1Parameters_.record(dbParameters_.getDb());

	unsigned int ifc2ParametersId = 
		ifc2Parameters_.record(dbParameters_.getDb());

	unsigned int ifc3ParametersId = 
		ifc3Parameters_.record(dbParameters_.getDb());

	unsigned int schedParametersId = 
		schedulerParameters_.record(dbParameters_.getDb());

	unsigned int dxParametersId =
		dxParameters_.record(dbParameters_.getDb());

	unsigned int testSig1ParametersId = 
		testSig1Parameters_.record(dbParameters_.getDb());

	unsigned int testSig2ParametersId = 
		testSig2Parameters_.record(dbParameters_.getDb());

	unsigned int testSig3ParametersId = 
		testSig3Parameters_.record(dbParameters_.getDb());

	unsigned int tscopeParametersId = 
		tscopeParameters_.record(dbParameters_.getDb());

	if (useTscope())
	{
		recordTscopeStatusInDatabase(tscopeStatusOnTarget_);
	}

	stringstream sqlstmt;
	sqlstmt.precision(PrintPrecision);    
	sqlstmt.setf(std::ios::fixed);  // show all decimal places up to precision

	sqlstmt << "UPDATE Activities SET "
		<< " type = '"     << getActivityType() << "'"

		<< ", actParametersId = " << actParametersId
		<< ", ifc1ParametersId = "  << ifc1ParametersId
		<< ", ifc2ParametersId = "  << ifc2ParametersId
		<< ", ifc3ParametersId = "  << ifc3ParametersId
		<< ", dxParametersId = " << dxParametersId
		<< ", schedParametersId = " << schedParametersId
		<< ", tsig1ParametersId = " << testSig1ParametersId  // TBD rename in DB?
		<< ", tsig2ParametersId = " << testSig2ParametersId  // TBD rename in DB?
		<< ", tsig3ParametersId = " << testSig3ParametersId  // TBD rename in DB?
		<< ", tscopeParametersId = " << tscopeParametersId

		<< ", length = " << dataCollectionLengthSecs_

		<< ", tuningaSkyFreqMhz = " << ataTuningSkyFreqMhz_[TSCOPE_TUNINGA]
		<< ", tuningbSkyFreqMhz = " << ataTuningSkyFreqMhz_[TSCOPE_TUNINGB]
		<< ", tuningcSkyFreqMhz = " << ataTuningSkyFreqMhz_[TSCOPE_TUNINGC]
		<< ", tuningdSkyFreqMhz = " << ataTuningSkyFreqMhz_[TSCOPE_TUNINGD]

		<< ", minDxSkyFreqMhz = " << getMinDxSkyFreqMhz()
		<< ", maxDxSkyFreqMhz = " << getMaxDxSkyFreqMhz()

		<< " where id= " << getId()
		<< " ";

	submitDbQueryWithThrowOnError(sqlstmt.str(), methodName, __LINE__);

}

void ObserveActivityImp::updateDbErrorComment(DbParameters & dbParam,
		const string& comment)
{
	const string methodName("updateDbErrorComment");

	try 
	{
		checkForZeroActId();

		stringstream sqlstmt;
		sqlstmt << "UPDATE Activities SET "
			<< "comments = '"
			<<  SseUtil::insertSlashBeforeSubString(comment, "'")
			<< "' where id = "
			<< getId()
			<< " ";

		submitDbQueryWithThrowOnError(dbParam, sqlstmt.str(), methodName, __LINE__);
	}
	catch (SseException &except) 
	{
		SseMessage::log(MsgSender, getId(),
				except.code(), except.severity(),
				except.descrip(), 
				except.sourceFilename(), except.lineNumber());
	}
}

void ObserveActivityImp::updateIfcStatus(const IfcStatus& ifcStatus) 
{
	const string methodName("updateIfcStatus");

	checkForZeroActId();

	string tableName("ifcStatus");

	stringstream sqlstmt;
	sqlstmt << "INSERT INTO " << tableName  << " SET "
		<< " ifc = '" << ifcStatus.name << "'" 
		<< ", actId = " << getId()
		<< ", attnL = " << ifcStatus.attnDbLeft
		<< ", attnR = " << ifcStatus.attnDbRight
		<< ", stxMeanL = " << ifcStatus.stxMeanLeft
		<< ", stxMeanR = " << ifcStatus.stxMeanRight
		<< ", stxVarL = " << ifcStatus.stxVarLeft
		<< ", stxVarR = " << ifcStatus.stxVarRight
		<< ", stxStatus = " << ifcStatus.stxStatus
		<< " ";

	submitDbQueryWithThrowOnError(sqlstmt.str(), methodName, __LINE__);

}

string ObserveActivityImp::prepareObsSummStatsSqlUpdateStmt(
		const ObsSummaryStats & obsSummaryStats)
{
	stringstream sqlStmt;

	sqlStmt 
		<< "passCwCohDetCandidates = " 
		<< obsSummaryStats.passCwCohDetCandidates
		<< ", confirmedCwCandidates = " 
		<< obsSummaryStats.confirmedCwCandidates
		<< ", confirmedPulseCandidates = " 
		<< obsSummaryStats.confirmedPulseCandidates
		<< ", allCwCandidates = " 
		<< obsSummaryStats.allCwCandidates
		<< ", allPulseCandidates = " 
		<< obsSummaryStats.allPulseCandidates
		<< ", cwSignals = " 
		<< obsSummaryStats.cwSignals
		<< ", pulseSignals = " 
		<< obsSummaryStats.pulseSignals
		<< ", testSignals = " 
		<< obsSummaryStats.testSignals
		<< ", zeroDriftSignals = " 
		<< obsSummaryStats.zeroDriftSignals
		<< ", recentRfiDatabaseMatches = " 
		<< obsSummaryStats.recentRfiDatabaseMatches
		<< ", unknownSignals = " 
		<< obsSummaryStats.unknownSignals
		<< " ";

	return sqlStmt.str();
}


void ObserveActivityImp::updateActivityStatistics()
{
	const string methodName("updateActivityStatistics");

	try 
	{
		checkForZeroActId();

		stringstream sqlstmt;

		sqlstmt << "UPDATE Activities SET "
			<< " startOfDataCollection = '" 
			<< SseUtil::isoDateTimeWithoutTimezone(startTime_.value())
			<< "'"
			<< ", validObservation = 'Yes',"
			<< prepareObsSummStatsSqlUpdateStmt(combinedObsSummaryStats_)
			<< " where id = " << getId()
			<< " ";

		submitDbQueryWithThrowOnError(sqlstmt.str(), methodName, __LINE__);

	}
	catch (SseException &except)
	{
		SseMessage::log(MsgSender, getId(),
				except.code(), except.severity(), except.descrip(),
				except.sourceFilename(), except.lineNumber());
	}

}


/*
   Get coordinates for the given target Id.
   If it's "moving" (i.e., a spacecraft)
   then the moving flag is set to true and the
   coordinates are set to zero.

   If it's not a spacecraft, the target's coordinates
   are adjusted by its proper motion; i.e., the epoch
   is changed to the current time.
   */
void ObserveActivityImp::getTargetInfo(
		MYSQL *conn,
		TargetId targetId,
		double *ra2000Rads,
		double *dec2000Rads,
		bool *moving)
{
	string methodName("::getTargetInfo");

	stringstream sqlstmt;
	stringstream errorMsg;

	sqlstmt << "select catalog, ra2000Hours, dec2000Deg, "
		<< "pmRaMasYr, pmDecMasYr, parallaxMas "
		<< "from TargetCat where targetId = " << targetId << " ";

	enum colIndices { catalogCol, ra2000HoursCol, dec2000DegCol,
		pmRaCol, pmDecCol, parallaxCol, numCols };

	MysqlQuery query(conn);
	query.execute(sqlstmt.str(), numCols, __FILE__, __LINE__);

	MYSQL_ROW row = mysql_fetch_row(query.getResultSet());
	if (mysql_num_rows(query.getResultSet()) > 1)
	{
		errorMsg << methodName << " found multiple rows of target id: "
			<< targetId << endl;;
		throw SseException(errorMsg.str(), __FILE__, __LINE__, 
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	if (!row)
	{
		errorMsg << methodName << " target Id: " << targetId
			<< " not found.\n";
		throw SseException(errorMsg.str(), __FILE__, __LINE__, 
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	// determine if this is a moving target
	string catalog(query.getString(
				row, catalogCol, __FILE__, __LINE__));
	const string spacecraftCatalog("spacecraft");
	if (catalog == spacecraftCatalog) 
	{
		*moving = true;
		*ra2000Rads = 0;
		*dec2000Rads = 0;

		return;
	}

	*moving = false;

	// apply proper motion in J2000 coords for current time
	double raOrigRads(SseAstro::hoursToRadians(
				query.getDouble(row, ra2000HoursCol, __FILE__, __LINE__)));

	double decOrigRads(SseAstro::degreesToRadians(
				query.getDouble(row, dec2000DegCol, __FILE__, __LINE__)));

	double pmRaMasYr(query.getDouble(row, pmRaCol, __FILE__, __LINE__));

	double pmDecMasYr(query.getDouble(row, pmDecCol, __FILE__, __LINE__));

	double parallaxMas(query.getDouble(row, parallaxCol,
				__FILE__, __LINE__));

	RaDec raDecOrig;
	raDecOrig.ra = Radian(raOrigRads);
	raDecOrig.dec = Radian(decOrigRads);
	TargetPosition pos(raDecOrig, pmRaMasYr, pmDecMasYr,
			parallaxMas);

	time_t currentTime(time(NULL));

	RaDec pmAdjustedRaDec = pos.positionAtNewEpoch(
			currentTime);

	*ra2000Rads = pmAdjustedRaDec.ra.getRadian();
	*dec2000Rads = pmAdjustedRaDec.dec.getRadian();
}


string ObserveActivityImp::getSpacecraftEphemFilename(MYSQL *conn, TargetId targetId)
{
	string methodName("::getSpacecraftEphemFilename");

	stringstream targetCatQuery;
	targetCatQuery << "select ephemFilename "
		<< "from Spacecraft WHERE ";
	targetCatQuery << " targetId = " << targetId;

	enum resultCols { ephemCol, numCols };

	stringstream errorMsg;
	MysqlQuery query(conn);
	query.execute(targetCatQuery.str(), numCols, __FILE__, __LINE__);

	MYSQL_ROW row = mysql_fetch_row(query.getResultSet());
	if (mysql_num_rows(query.getResultSet()) > 1)
	{
		errorMsg << methodName << " found multiple rows of target id: "
			<< targetId << endl;;
		throw SseException(errorMsg.str(), __FILE__, __LINE__, 
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	if (!row)
	{
		errorMsg << methodName << " target Id: " << targetId
			<< " not found.\n";
		throw SseException(errorMsg.str(), __FILE__, __LINE__, 
				SSE_MSG_DBERR, SEVERITY_ERROR);
	};

	string ephemFilename(query.getString(row, ephemCol,
				__FILE__, __LINE__));

	return ephemFilename;
}


void ObserveActivityImp::storeTuningNameForPreludeBeam(const string &preludeBeamName,
		const string &tuningName)
{
	preludeBeamToAtaTuningNameMap_.insert(make_pair(preludeBeamName,
				tuningName));
}

string ObserveActivityImp::getTuningNameForPreludeBeam(const string &preludeBeamName)
{
	string tuningName("");

	PreludeBeamToAtaTuningNameMap::iterator it = 
		preludeBeamToAtaTuningNameMap_.find(preludeBeamName);
	if (it != preludeBeamToAtaTuningNameMap_.end())
	{
		tuningName = it->second;
	}

	return tuningName;
}


void ObserveActivityImp::recordPointRequestInDatabase(
		int beamNumber,
		const string &beamName,
		const TscopePointing & pointing)
{
	const string methodName("recordPointRequestInDatabase");
	checkForZeroActId();

	string tableName("TscopePointReq");	
	stringstream sqlstmt;
	sqlstmt.precision(PrintPrecision);    
	sqlstmt.setf(std::ios::fixed);  // show all decimal places up to precision

	sqlstmt << "INSERT INTO " << tableName << " SET "
		<< " actId = " << getId()
		<< ", beamNumber = " << beamNumber
		<< ", ataBeam = '" << SseUtil::strToLower(beamName)
		<< "'"
		<< ", coordType = '" 
		<< SseTscopeMsg::coordSysToString(pointing.coordSys)
		<< "'"
		<< ", azDeg = " << pointing.azDeg      
		<< ", elDeg = " << pointing.elDeg        
		<< ", raHours = " << pointing.raHours      
		<< ", decDeg = " << pointing.decDeg       
		<< ", galLongDeg = " << pointing.galLongDeg     
		<< ", galLatDeg = " << pointing.galLatDeg      
		<< " ";

	submitDbQueryWithThrowOnError(sqlstmt.str(), methodName, __LINE__);

}

void ObserveActivityImp::recordTscopeStatusInDatabase(
		const TscopeStatusMultibeam & multibeamStatus)
{
	// record position of all beams in use
	bool first(true);
	for (vector<TscopeBeam>::iterator it = ataBeamsInUse_.begin();
			it != ataBeamsInUse_.end(); ++it)
	{
		TscopeBeam ataBeam = *it;

		/* record primaryPointing for first beam only,
		   assumes all synth beams share the same primary 
		   */
		if (first)
		{
			recordBeamStatusInDatabase(PrimaryBeam,
					multibeamStatus.primaryPointing[ataBeam]);

			recordSubarrayStatusInDatabase(PrimaryBeam, 
					multibeamStatus.subarray[ataBeam]);

			first = false;
		}

		string beamName(SseUtil::strToLower(SseTscopeMsg::beamToName(ataBeam)));
		recordBeamStatusInDatabase(beamName, multibeamStatus.synthPointing[ataBeam]);
	}
}

void ObserveActivityImp::recordBeamStatusInDatabase(
		const string & beamName, const TscopePointing & pointing)
{
	const string methodName("recordBeamStatusInDatabase");
	checkForZeroActId();

	string tableName("TscopeBeamStatus");	
	stringstream sqlstmt;
	sqlstmt.precision(PrintPrecision);    
	sqlstmt.setf(std::ios::fixed);  // show all decimal places up to precision

	sqlstmt << "INSERT INTO " << tableName << " SET "
		<< " actId = " << getId()
		<< ", ataBeam = '" 
		<< beamName
		<< "'"
		<< ", coordType = '" << SseTscopeMsg::coordSysToString(pointing.coordSys)
		<< "'"
		<< ", azDeg = " << pointing.azDeg      
		<< ", elDeg = " << pointing.elDeg        
		<< ", raHours = " << pointing.raHours      
		<< ", decDeg = " << pointing.decDeg       
		<< ", galLongDeg = " << pointing.galLongDeg     
		<< ", galLatDeg = " << pointing.galLatDeg      
		<< " ";

	submitDbQueryWithThrowOnError(sqlstmt.str(), methodName, __LINE__);
}

void ObserveActivityImp::recordSubarrayStatusInDatabase(
		const string & beamName, const TscopeSubarrayStatus & status)
{
	const string methodName("recordSubarrayStatusInDatabase");
	checkForZeroActId();

	string tableName("TscopeSubarrayStatus");	
	stringstream sqlstmt;
	sqlstmt.precision(PrintPrecision);    
	sqlstmt.setf(std::ios::fixed);  // show all decimal places up to precision

	// TBD update rest of status fields

	sqlstmt << "INSERT INTO " << tableName << " SET "
		<< " actId = " << getId()
		<< ", ataBeam = '" 
		<< beamName << "'"
		<< ", numTotal = " << status.numTotal         
		<< ", numSharedPointing = " << status.numSharedPointing
		<< ", numTrack = " << status.numTrack         
		<< ", numSlew = " <<  status.numSlew          
		<< ", numStop = " << status.numStop          
		<< ", numOffline = " << status.numOffline       
		<< ", numDriveError = " << status.numDriveError    
		<< ", zfocusMhz = " << status.zfocusMhz 
		<< ", gcErrorDeg = " << status.gcErrorDeg 
		<< ", wrap = " << status.wrap 
		<< " ";

	submitDbQueryWithThrowOnError(sqlstmt.str(), methodName, __LINE__);
}



// ------------------------------------------------

void ObserveActivityImp::submitDbQueryWithLoggingOnError(
		const string &sqlStmt,
		const string &callingMethodName,
		int lineNumber)
{
	try 
	{
		submitDbQueryWithThrowOnError(sqlStmt, callingMethodName,
				lineNumber);
	}
	catch (SseException &except)
	{
		SseMessage::log(MsgSender, getId(), SSE_MSG_DBERR,
				SEVERITY_WARNING, except.descrip(),
				__FILE__, lineNumber);
	}

}

void ObserveActivityImp::submitDbQueryWithThrowOnError(
		const string &sqlStmt,
		const string &callingMethodName,
		int lineNumber)
{
	submitDbQueryWithThrowOnError(dbParameters_,
			sqlStmt,
			callingMethodName,
			lineNumber);
}


void ObserveActivityImp::submitDbQueryWithThrowOnError(
		DbParameters &dbParam,
		const string &sqlStmt,
		const string &callingMethodName,
		int lineNumber)
{
	if (!dbParam.useDb())
	{
		throw SseException(
				" submitDbQuery::error: database is turned off\n",
				__FILE__, lineNumber, SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	if (!dbParam.getDb())
	{
		throw SseException(
				" submitDbQuery::error: NULL db pointer\n",
				__FILE__, lineNumber, SSE_MSG_DBERR, SEVERITY_ERROR);
	}

	if (mysql_query(dbParam.getDb(), sqlStmt.c_str()) != 0)
	{	
		stringstream strm;
		strm << callingMethodName 
			<< " submitDbQuery: MySQL error: " 
			<< mysql_error(dbParameters_.getDb())  << endl;

		throw SseException(strm.str(), __FILE__, lineNumber,
				SSE_MSG_DBERR, SEVERITY_ERROR);
	}
}
