/*******************************************************************************

 File:    targetgrid.cpp
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


// generate grid of 1/2 FWHM beamsize target pointings around
// given ra/dec

// input:  RA Deg, Dec deg, freq Mhz

#include "SseUtil.h"
#include <iostream>
#include <sstream>
#include <cstdlib>

using namespace std;

void printUsage();
void printGrid(double beamScale, double targetRaDeg, double targetDecDeg, double targetFreqMhz);

int main(int argc, char *argv[])
{
   if (argc != 4)
   {
      printUsage();
      exit(1);
   }

   try 
   {
      double targetRaDeg = SseUtil::strToDouble(argv[1]);
      double targetDecDeg = SseUtil::strToDouble(argv[2]);
      double targetFreqMhz = SseUtil::strToDouble(argv[3]);

      double beamScale = 0.5;
      printGrid(beamScale, targetRaDeg, targetDecDeg, targetFreqMhz);

      beamScale = 1.0;
      printGrid(beamScale, targetRaDeg, targetDecDeg, targetFreqMhz);

      beamScale = 2.0;
      printGrid(beamScale, targetRaDeg, targetDecDeg, targetFreqMhz);

      beamScale = 3.0;
      printGrid(beamScale, targetRaDeg, targetDecDeg, targetFreqMhz);

   }
   catch (SseException & except)
   {
      cout << except << endl;
      exit(1);
   }
   exit(0);
}

void printUsage()
{
   cout << "args:  <RA Deg> <Dec deg> <freq MHz>" << endl;
}

void printGrid(double beamScale, double targetRaDeg, double targetDecDeg, double targetFreqMhz)
{
   stringstream strm;
   strm.precision(6);   // show N places after the decimal
   strm.setf(std::ios::fixed);  // show all decimal places up to precision

   strm << "orig target: ra = " << targetRaDeg  << " deg " 
	<< " dec = " << targetDecDeg <<  " deg " << endl;

   const double AtaBeamsizeAt1GhzDeg = 3.5;
   const double MhzPerGhz = 1000;
   double targetFreqGhz = targetFreqMhz / MhzPerGhz;
   double beamsizeDeg = AtaBeamsizeAt1GhzDeg / targetFreqGhz;

   strm << "beamsize at " << targetFreqMhz << "Mhz is " 
	<< beamsizeDeg  << " deg " << endl;

   // 3x3 grid in ra, dec

   double beamOffsetPerStepDeg = beamsizeDeg * beamScale;

   strm << "beam offset per step is " << beamOffsetPerStepDeg << " deg" << endl;

   double lowerLeftRaDeg = targetRaDeg - beamOffsetPerStepDeg;
   double lowerLeftDecDeg = targetDecDeg - beamOffsetPerStepDeg;


   int nsteps = 4;

   double outRaDeg = lowerLeftRaDeg;

   strm << "\traDeg\tdecDeg" << endl;
   int count = 1;
   for (int raStep = 0; raStep < nsteps; raStep++)
   {
      double outDecDeg = lowerLeftDecDeg;
      for (int decStep = 0; decStep < nsteps; decStep++)
      {
	 strm << count++ << ") " <<  outRaDeg << "\t" << outDecDeg << endl;

	 outDecDeg += beamOffsetPerStepDeg;
      }
      outRaDeg += beamOffsetPerStepDeg;
   }

   cout << strm.str() << "\n\n" << endl;

}

