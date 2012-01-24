/*******************************************************************************

 File:    DxParameters.h
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


#ifndef DxParameters_H
#define DxParameters_H

#include "SeekerParameterGroup.h"

#include <mysql.h>

class DxParametersInternal;
struct DxActivityParameters;

class DxParameters : public SeekerParameterGroup {

 public:

  DxParameters(string command);  // Must pass by value to avoid static init segfault
  DxParameters(const DxParameters& rhs);
  DxParameters& operator=(const DxParameters& rhs);

  ~DxParameters();

  const DxActivityParameters &getDxActParamStruct() const;
  void dumpstruct() const;

  // get needed fields
  int getDataCollectionLengthSecs() const;
  bool useManuallyAssignedDxBandwidth() const;
  bool allowPulseDetection() const;
  bool recentRfiEnable() const;
  double getRecentRfiMaskElementWidthMinHz() const;
  int getRecentRfiMaskSizeMax() const;
  int getDataRequestMaxCompampSubchannels() const;

  // immediate commands
  const char *names() const;
  const char *status(const char *dxName) const;
  const char *reqstat(const char *dxName);
  const char *intrin(const char *dxName) const;
  const char *config(const char *dxName);
  const char *senddatareq(const char *dxName);
  const char *reqfreq(double rfFreq, const char *dxName);
  const char *reqsub(int subchannel, const char *dxName);
  const char *shutdown(const char *dxName0,
		       const char *dxName1,
		       const char *dxName2,
		       const char *dxName3,
		       const char *dxName4,
		       const char *dxName5,
		       const char *dxName6,
		       const char *dxName7,
		       const char *dxName8,
		       const char *dxName9);
  const char *stop(const char *dxName);
  const char *restart(const char *dxName);
  const char *resetsocket(const char *dxName);
  const char *load(const char *paramName, const char *paramValue, 
		   const char* dxName);

protected:
  virtual void addParameters();
  virtual void addAllImmedCmdHelp();

 private: 
  DxParametersInternal *internal_;


};

#endif // DxParameters_H
