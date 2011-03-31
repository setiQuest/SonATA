/*******************************************************************************

 File:    ChildClusterer.cpp
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
// Clustering base class
//
// $Header: /home/cvs/nss/sonata-pkg/dx/lib/ChildClusterer.cpp,v 1.1 2009/03/06 21:56:35 kes Exp $
//

#include "System.h"
#include "ChildClusterer.h"
#include "SuperClusterer.h"

#define EDG_MODNAME "Clusterer"
#include "edg_trace.h"

namespace dx {

TR_DECLARE(detail);

ChildClusterer::ChildClusterer(SuperClusterer *parent_ )
	: parent(parent_)
	, activityId(-1)
	, spectraPerObs(64)
	, baseFreq(1.0)
	, highFreq(1.0)
	, binWidth(1.0)
	, secondsPerObs(spectraPerObs/binWidth)
	, complete(false)
{
	parent->attach(this);
}

ChildClusterer::~ChildClusterer()
{
	parent->detach(this);
}

void
ChildClusterer::setObsParams(int32_t activityId_, int nSpectra_,
		double baseFreq_, double chanWidth_, double binWidth_)
{
	activityId = activityId_;
	spectraPerObs = nSpectra_; // XXX adjust for resolution ???
	baseFreq = baseFreq_;
	highFreq = baseFreq_ + chanWidth_;
	binWidth = binWidth_; // XXX adjust for resolution
	secondsPerObs = (spectraPerObs/2)/binWidth;
}

void
ChildClusterer::allHitsLoaded()
{
	complete = true;
}

void
ChildClusterer::clearHits()
{
	complete = false;
}

CwClusterer *
ChildClusterer::getCw()
{
	return 0;
}

PulseClusterer *
ChildClusterer::getPulse()
{
	return 0;
}

}

