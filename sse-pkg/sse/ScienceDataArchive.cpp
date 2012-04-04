/*******************************************************************************

 File:    ScienceDataArchive.cpp
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



// Class that stores science data (baselines & complex amplitudes)

#include "ScienceDataArchive.h" 
#include "WriteScienceData.h"
#include "SseUtil.h"
#include "SseArchive.h"
#include "Assert.h"
#include <algorithm>
#include <sstream>

using namespace std;

ScienceDataArchive::ScienceDataArchive(const string &archiveFilenamePrefix,
				       const string &dxName)
{
    // form the output filenames based on the archiveFilenamePrefix
    archiveFilenamePrefix_ = archiveFilenamePrefix;

    // NSS baselines & complex amplitudes
    nssBaselinesOutFilenameL_ = archiveFilenamePrefix_ + "L.baseline";
    nssBaselinesOutFilenameR_= archiveFilenamePrefix_ + "R.baseline";
    nssCompampsOutFilenameL_ = archiveFilenamePrefix_ + "L.compamp";
    nssCompampsOutFilenameR_ = archiveFilenamePrefix_ + "R.compamp";

    // Monitored confirmation data (overwritten for each activity)
    string confirmDataPrefix = SseArchive::getConfirmationDataDir() +
	dxName + "-";

    // compamps 
    nssCompampsMonitorOutFilenameL_ = confirmDataPrefix + "L.compamp";
    nssCompampsMonitorOutFilenameR_ = confirmDataPrefix + "R.compamp";

    // baselines
    nssBaselinesMonitorOutFilenameL_ = confirmDataPrefix + "L.baseline";
    nssBaselinesMonitorOutFilenameR_ = confirmDataPrefix + "R.baseline";


}

ScienceDataArchive::~ScienceDataArchive()
{
}

void ScienceDataArchive::truncateOutputFiles()
{
    // truncate sci data output files, if they exist
    SseUtil::truncateFile(nssBaselinesOutFilenameL_);
    SseUtil::truncateFile(nssBaselinesOutFilenameR_);
    SseUtil::truncateFile(nssCompampsOutFilenameL_);
    SseUtil::truncateFile(nssCompampsOutFilenameR_);

    SseUtil::truncateFile(nssCompampsMonitorOutFilenameL_);
    SseUtil::truncateFile(nssCompampsMonitorOutFilenameR_);
    SseUtil::truncateFile(nssBaselinesMonitorOutFilenameL_);
    SseUtil::truncateFile(nssBaselinesMonitorOutFilenameR_);


}

void ScienceDataArchive::storeBaseline(const BaselineHeader &hdr,
		       const BaselineValue valueArray[])
{
    // cout << hdr << endl;

    // TBD FIX ME
    // This is a hack to shoehorn the variable length 
    // baselines into the old fixed length ones.
    // Put the Baseline on the heap to avoid a purify stack checking bug.

    Baseline *baselineBuffer = new Baseline;
    Baseline & baseline = *baselineBuffer;
    baseline.header = hdr;
    baseline.header.numberOfSubchannels = MAX_BASELINE_SUBCHANNELS;

    // zero out the fixed sized output array
    for (int i=0; i<MAX_BASELINE_SUBCHANNELS; ++i)
    {
	baseline.baselineValues[i] = 0;
    }

    // copy over the input baseline values, up to the buffer size
    int nSubchannelsToCopy = min(hdr.numberOfSubchannels, MAX_BASELINE_SUBCHANNELS);

    // copy data values
    Assert (nSubchannelsToCopy >=0);
    for (int i=0; i<nSubchannelsToCopy; ++i)
    {
	baseline.baselineValues[i] = valueArray[i].value;
    }
    

    //cout << "ScienceDataArchive::storeBaseline" << endl;
    //SseDxMsg::printBaseline(baseline);

    int numberOfBaselineValues = hdr.numberOfSubchannels;

    if (baseline.header.pol == POL_RIGHTCIRCULAR)
    {
	// do these last, since the marshalling will mess up the
	// buffer for anything other than a pure binary dump (no use 
	// of the header fields allowed).

	// only write the part of the array that's filled in
	baseline.header.numberOfSubchannels = numberOfBaselineValues;

	baseline.marshall();

	// TBD: clean this up when the deprecated, fixed length Baseline struct is removed

	writeVariableLengthBaselineToNssFile(baseline.header, 
					     baseline.baselineValues,
					     numberOfBaselineValues, 
					     nssBaselinesOutFilenameR_);

	writeVariableLengthBaselineToNssFile(baseline.header, 
					     baseline.baselineValues,
					     numberOfBaselineValues, 
					     nssBaselinesMonitorOutFilenameR_);
    }
    else
    {
	// do these last, since the marshalling will mess up the
	// buffer for anything other than a pure binary dump (no use 
	// of the header fields allowed).

	// only write the part of the array that's filled in
	baseline.header.numberOfSubchannels = numberOfBaselineValues;

	baseline.marshall();

	writeVariableLengthBaselineToNssFile(baseline.header, 
					     baseline.baselineValues,
					     numberOfBaselineValues, 
					     nssBaselinesOutFilenameL_);

	writeVariableLengthBaselineToNssFile(baseline.header, 
					     baseline.baselineValues,
					     numberOfBaselineValues, 
					     nssBaselinesMonitorOutFilenameL_);
    }

    delete baselineBuffer;
}


// copy the hdr & subchannel into the fixed size ca structure
static void extractSubchannel(ComplexAmplitudes &ca,
			   const ComplexAmplitudeHeader &hdr,
			   const SubchannelCoef1KHz subchannel)
{
	ca.header = hdr;
	ca.header.numberOfSubchannels = 1;

	// initialize the alignment padding so that a binary comparison
	// of the structure doesn't fail
//	ca.header.activityId = NSS_NO_ACTIVITY_ID;

	// copy over the data array
	for (int j=0; j<MAX_SUBCHANNEL_BINS_PER_1KHZ_HALF_FRAME; ++j)
	{
	    ca.compamp.coef[j] = subchannel.coef[j];
	}
}

void ScienceDataArchive::storeComplexAmplitudes(
    const ComplexAmplitudeHeader &hdr,
    const SubchannelCoef1KHz subchannelArray[])
{
//	cout << "storeComplexAmplitudes, hdr: ";
//	cout << hdr;

    // go through variable length array, writing out a header
    // with each subchannel (for backwards compatibility with the
    // write routines til we get a chance to adjust them).

    int nSubchannels = hdr.numberOfSubchannels;
    if (hdr.startSubchannelId == currentDataRequestSubchannel_ )
    {
    for (int subchannel=0; subchannel < nSubchannels; ++subchannel)
    { 
	ComplexAmplitudes compamp;
	extractSubchannel(compamp, hdr, subchannelArray[subchannel]);

	//cout << "scienceDataArchive::storeComplexAmplitudes" << endl;
	//SseDxMsg::printComplexAmplitudes(compamp);

//	cout << "compamp header: " << compamp.header;

	if (compamp.header.pol == POL_RIGHTCIRCULAR)
	{
	    compamp.marshall();
	    writeCompAmpsToNssFile(compamp, nssCompampsMonitorOutFilenameR_);
	    writeCompAmpsToNssFile(compamp, nssCompampsOutFilenameR_);
	}
	else
	{
	    compamp.marshall(); 
	    writeCompAmpsToNssFile(compamp, nssCompampsMonitorOutFilenameL_);
	    writeCompAmpsToNssFile(compamp, nssCompampsOutFilenameL_);
	}

    }
    }
    // Now check if this is one of the SETILive subchannels
    
    string liveFilename = getLiveFilename( hdr.pol, hdr.startSubchannelId);
    if (liveFilename != "")
    {
	ComplexAmplitudes compamp;
	extractSubchannel(compamp, hdr, subchannelArray[0]);
	    compamp.marshall(); 
	    writeCompAmpsToNssFile(compamp, liveFilename );
    }
}

void ScienceDataArchive::prepareCompampSubchannelFiles(
		const string &archiveFilenamePrefix,
                     const string &dxName, const int targetId,
                     const vector<int> & compampSubchannels,
                     const vector<int> & candIds)
{
	for ( unsigned int subchanIndex = 0; 
			subchanIndex < compampSubchannels.size();
			++subchanIndex)
	{
		stringstream targetSubchan;
		targetSubchan << "tg" << targetId << "." << "sbch" << 
			compampSubchannels[subchanIndex] 
			<< ".requested.id" << candIds[subchanIndex] << ".";
		CompampSubchannelFile subchanFile;
		subchanFile.subchanNumber = compampSubchannels[subchanIndex];
		subchanFile.pol = POL_RIGHTCIRCULAR;
		subchanFile.filename = archiveFilenamePrefix +
			targetSubchan.str() + "R.compamp";
		//cout << subchanFile.filename << endl;
		compampSubchannelFileList_.push_back(subchanFile);
		subchanFile.pol = POL_LEFTCIRCULAR;
		subchanFile.filename = archiveFilenamePrefix +
			targetSubchan.str() + "L.compamp";
		//cout << subchanFile.filename << endl;
		compampSubchannelFileList_.push_back(subchanFile);
		
	}
}

void ScienceDataArchive::prepareCompampSubchannelFiles(
		const string &archiveFilenamePrefix,
                     const string &dxName, const int targetId,
                          const vector<int> & compampSubchannels)
{
	for ( unsigned int subchanIndex = 0; 
			subchanIndex< compampSubchannels.size();
			++subchanIndex)
	{
		stringstream targetSubchan;
		targetSubchan << "tg" << targetId << "." << "sbch" << 
			compampSubchannels[subchanIndex] << "." << "requested.";
		CompampSubchannelFile subchanFile;
		subchanFile.subchanNumber = compampSubchannels[subchanIndex];
		subchanFile.pol = POL_RIGHTCIRCULAR;
		subchanFile.filename = archiveFilenamePrefix +
			targetSubchan.str() + "R.compamp";
		//cout << subchanFile.filename << endl;
		compampSubchannelFileList_.push_back(subchanFile);
		subchanFile.pol = POL_LEFTCIRCULAR;
		subchanFile.filename = archiveFilenamePrefix +
			targetSubchan.str() + "L.compamp";
		//cout << subchanFile.filename << endl;
		compampSubchannelFileList_.push_back(subchanFile);
		
	}
}

string ScienceDataArchive::getLiveFilename( Polarization pol, int subchan )
{
	for (CompampSubchannelFileList::iterator 
			index = compampSubchannelFileList_.begin();
			index != compampSubchannelFileList_.end();
			++index)
	{
		if ( index->subchanNumber == subchan &&
			index->pol == pol)
			return( index->filename );
	}
	return( "");
}
void ScienceDataArchive::setCurrentDataRequestSubchannel( int subchan )
{
	currentDataRequestSubchannel_ = subchan;
}
