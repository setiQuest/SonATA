/*******************************************************************************

 File:    ChannelizerParameters.h
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


#ifndef ChannelizerParameters_H
#define ChannelizerParameters_H

#include "SeekerParameterGroup.h"

#include <mysql.h>

class ChannelizerParametersInternal;
struct ChannelizerParameters;

class ChannelizerParameters : public SeekerParameterGroup {

 public:

  ChannelizerParameters(string command);
  ChannelizerParameters(const ChannelizerParameters& rhs);
  ChannelizerParameters& operator=(const ChannelizerParameters& rhs);
  int32_t getDelay();

  ~ChannelizerParameters();

  // immediate commands
  const char *names() const;
  const char *status(const char *chanName) const;
  const char *reqstat(const char *chanName);
  const char *intrin(const char *chanName) const;
  const char *shutdown(const char *chanName);
  const char *resetsocket(const char *chanName);
  const char *restart(const char *chanName);
  const char *start(int delaySecs, double skyFreqMhz, const char *chanName);
  const char *stop(const char *chanName);

protected:
  virtual void addParameters();
  virtual void addAllImmedCmdHelp();

 private: 
  ChannelizerParametersInternal *internal_;


};

#endif 
