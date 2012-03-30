/*******************************************************************************

 File:    ScienceDataArchive.h
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


#ifndef ScienceDataArchive_H
#define ScienceDataArchive_H

#include "sseDxInterface.h"
#include <string>
#include <list>
#include <vector>
using std::string;
using std::vector;

struct compampFile
{
	int subchanNumber;
	Polarization pol;
	string filename;
} typedef CompampSubchannelFile;

    typedef std::list<CompampSubchannelFile> CompampSubchannelFileList;

class ScienceDataArchive
{
 public:
    ScienceDataArchive(const string &archiveFilenamePrefix,
		       const string &dxName);
    virtual ~ScienceDataArchive();

    void storeBaseline(const BaselineHeader &hdr,
		       const BaselineValue valueArray[]);

    void storeComplexAmplitudes(const ComplexAmplitudeHeader &hdr,
				     const SubchannelCoef1KHz subchannelArray[]);

    void storeSignalArchiveData(const ArchiveDataHeader &hdr,
				const SubchannelCoef1KHz subchannelArray[]);

    void truncateOutputFiles();

    void prepareCompampSubchannelFiles(const string &archiveFilenamePrefix,
		    const string &dxName, const int targetId,
		     const vector<int> & compampSubchannels);

    void prepareCompampSubchannelFiles(const string &archiveFilenamePrefix,
		    const string &dxName, const int targetId,
		     const vector<int> & compampSubchannels,
		     const vector<int> & candIds);

    string getLiveFilename( Polarization pol, int subchan);
    void setCurrentDataRequestSubchannel( int subchan);

 private:
    // Disable copy construction & assignment.
    // Don't define these.
    ScienceDataArchive(const ScienceDataArchive& rhs);
    ScienceDataArchive& operator=(const ScienceDataArchive& rhs);

    string archiveFilenamePrefix_;

    string nssBaselinesOutFilenameL_;
    string nssBaselinesOutFilenameR_;
    string nssCompampsOutFilenameL_;
    string nssCompampsOutFilenameR_;

// Container for the list of compamp Subchannels
    CompampSubchannelFileList compampSubchannelFileList_;

    // The Raw files appear not to be used
    string nssCompampsRaw1HzOutFilenameL_;
    string nssCompampsRaw1HzOutFilenameR_;
    // The monitor files go to ~/sonata_archive/confirmdata
    string nssCompampsMonitorOutFilenameL_;
    string nssCompampsMonitorOutFilenameR_;
    string nssBaselinesMonitorOutFilenameL_;
    string nssBaselinesMonitorOutFilenameR_;

    int currentDataRequestSubchannel_;
};

#endif // ScienceDataArchive_H
