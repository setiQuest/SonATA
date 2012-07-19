/*******************************************************************************

 File:    ssePorts.h
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

/*****************************************************************
 * sseports.h - Port number used for communication with sse
 * PURPOSE:  
 *****************************************************************/

#ifndef SSEPORTS_H
#define SSEPORTS_H

#define DEFAULT_DX_PORT "8888"
#define DEFAULT_ZX_PORT "8890"
#define DEFAULT_IFC_PORT "8899"
#define DEFAULT_TSIG_PORT "6677"
#define DEFAULT_TSCOPE_PORT "5577"
#define DEFAULT_DX_ARCHIVER_TO_SSE_PORT "8850"
#define DEFAULT_DX_TO_DX_ARCHIVER1_PORT "8857"
#define DEFAULT_DX_TO_DX_ARCHIVER2_PORT "8858"
#define DEFAULT_DX_TO_DX_ARCHIVER3_PORT "8859"
#define DEFAULT_COMPONENT_CONTROL_PORT "8866"
#define DEFAULT_CHANNELIZER_PORT "8870"

#define DEFAULT_MULTICASTADDR "230.1.2.3"

#endif /* SSEPORTS_H */
