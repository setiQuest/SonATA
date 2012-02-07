/*******************************************************************************

 File:    SetiLiveSignalInfo.h
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


#ifndef SetiLiveSignalInfo_H
#define SetiLiveSignalInfo_H

#include <string>
#include <sseDxInterface.h>
#include <list>

using std::string;
using std::list;

class SetiLiveSignalInfo
{
 public:
    SetiLiveSignalInfo();
    virtual ~SetiLiveSignalInfo();

    enum SignalType { SigType_CwCoherent, SigType_CwPower, 
		  SigType_Pulse, SigType_Uninit};

    static SignalType stringToSignalType(const string &typeString);

    string getSignalTypeString();

    SignalDescription setiLiveSignal;
    ConfirmationStats cfm;
    SignalType signalType;

 private:

// default copy ctor & assigment should be ok
#if 0
    // Disable copy construction & assignment.
    // Don't define these.
    SetiLiveSignalInfo(const SetiLiveSignalInfo& rhs);
    SetiLiveSignalInfo& operator=(const SetiLiveSignalInfo& rhs);
#endif

};

typedef list<SetiLiveSignalInfo> SetiLiveSignalInfoList;

#endif // SetiLiveSignalInfo_H
