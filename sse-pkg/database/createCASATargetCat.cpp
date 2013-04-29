/*******************************************************************************

 File:    createRaDecRegionTargetCat.cpp
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


// Create a target catalog for a selected ra dec range.
// Output is suitable for loading into the seeker TargetCat database table.

#include <iostream>
#include <cmath>
#include <string>

using namespace std;

void printDbTableLine(int targetId, double raHours, double decDeg,
                      int primaryTargetId, string autoschedule, string targetName);

double degreesToRadians(double degrees);

int main(int argc, char *argv[])
{
   // 1.6 deg Dec, x 4.2 deg RA  box containing rosetta

   // on 5 dec 2006, ra= 15.50 hr, dec= -19.667 deg
   // on 3 jan 2007, ra= 17.29311 hr, dec = -23.90168 deg
   // on 4 jan 2007, ra= 17.3527080 dec = -23.9631938

/*
   double targetRaHours = 17.3527080;
   double targetDecDeg =  -23.9631938;

   double raBoxWidthDeg = 4.2;
   double decBoxWidthDeg = 1.6;
*/

   /*
     Cassini position on 4 Dec 2008  12:00 UT
    RA=11.49886756 hr, dec =  5.3391801 deg
    Want 8x8 grid of 1 arcmin spacing

    1 arcmin ~= 0.01667 deg
    (dec near zero deg so ignore ra cos factor)

    boxwidthdeg = 8 * 0.01667 = 0.13336 deg

    */
   /*
    * CasA RA, Dec = 23.391, 58.808
    * 0.1 degree angular diameter.
    */
   //FILL IN THESE 7 VALUES
   double targetRaHours = 23.391;
   double targetDecDeg =   58.808;
   double raBoxWidthDeg = 0.1;
   double decBoxWidthDeg = 0.1;
   double skyFreqGhz = 6.667; 
   string targetName("CasA Grid");
   int firstTargetId = 40000;  // skip over spacecraft & old starlist.

   double arcMinInDeg =  0.01667;
   double raBoxWidthHours = raBoxWidthDeg / 15;
   double startRaHours = targetRaHours - (raBoxWidthHours / 2.0);
   double endRaHours = targetRaHours + (raBoxWidthHours / 2.0);
   double startDecDeg = targetDecDeg + (decBoxWidthDeg / 2.0);
   double endDecDeg = targetDecDeg - (decBoxWidthDeg / 2.0);
   if (startDecDeg > endDecDeg)
   {
      swap(startDecDeg, endDecDeg);
   }

   const double DegPerHour = 15.0;
    
//   double ataBeamsizeDegAtOneGhz = 3.5;
//   double beamsizeDeg = ataBeamsizeDegAtOneGhz / skyFreqGhz;

   double beamsizeDeg = arcMinInDeg;
   double beamsizeHours = beamsizeDeg / DegPerHour;

   cout.precision(6);           // show N places after the decimal
   cout.setf(std::ios::fixed);  // show all decimal places up to precision

   int targetId = firstTargetId;
   

   //double beamOverlapPercent = 0.15;
   double beamOverlapPercent = 0.02;

   cout << "-- $"   // split over 2 lines so does not get expanded here
        << "Id$ --" << endl
        << "-- ATA selected ra dec region pointing data" << endl
        << "-- for loading into the seeker TargetCat database table" << endl
        << " " << endl
        << "-- skyFreqGhz: " <<  skyFreqGhz << endl
        << "-- beamsize deg: " << beamsizeDeg << endl
        << "-- beam overlap percent: " << beamOverlapPercent << endl
        << "-- RA hours:  start: " << startRaHours 
        << " end: " << endRaHours << endl
        << "-- Dec deg:  start: " << startDecDeg 
        << " end: " << endDecDeg << endl
        << endl;

   // assign box center as primary pointing
   int primaryTargetId = -1;
   string autoschedule("No");
   printDbTableLine(targetId, targetRaHours, targetDecDeg, primaryTargetId,
                    autoschedule, targetName);

   // all the others get this as there primary target id
   primaryTargetId = targetId++;

   autoschedule="Yes";

   double beamNonOverlapPercent(1.0 - beamOverlapPercent);
   double decDegIncrement = beamsizeDeg * beamNonOverlapPercent;
   int decCount = 0;
   for (double decDeg = startDecDeg; decDeg < endDecDeg; 
        decDeg+=decDegIncrement)
   {	
      // account for smaller circles in RA as Dec increases
      double raHoursIncrement = beamsizeHours / 
         cos(degreesToRadians(decDeg)) * beamNonOverlapPercent;
    
      // alternate RA start position by 1/2 the RA increment
      // to more evenly distribute pointings from Dec row to row
	
      decCount++;
      double startRowRaHours = startRaHours;
      if (decCount % 2 == 0)
      {
         startRowRaHours += raHoursIncrement / 2;
      }

      for (double raHours = startRowRaHours; raHours < endRaHours;
           raHours+=raHoursIncrement)
      {
         //cout << targetId << "), " << raHours << ", " << decDeg << ", " << endl;
         printDbTableLine(targetId, raHours, decDeg, primaryTargetId,
                          autoschedule, targetName);
         targetId++;
	    
      }



   }
    
}


double degreesToRadians(double degrees)
{
  const double radiansPerDegree = 2.0 * M_PI/360.0;
  return (degrees * radiansPerDegree);

}

void printDbTableLine(int targetId, double raHours, double decDeg,
                      int primaryTargetId, string autoschedule, string targetName)
{
    string catalog("radecgrid"); 
    //string targetName("");
    int mura(0);
    int mudec(0);
    string spectralType("");

    // set parallax something reasonable
    // 1 arcsec == 1 parsec (~3.26 LY)
    // Make nonzero so LY calculation in SSE does not go to infinity.
    double parallax(0.1);  
   
    int bMag(0);
    int vMag(0);
    string ephemFilename("");
    string remarks("");

    cout << "INSERT INTO TargetCat (targetId, catalog, "
	 << "ra2000Hours, dec2000Deg, pmRaMasYr, pmDecMasYr, "
	 << "parallaxMas, spectralType, "
	 << "bMag, vMag, "
         << "aliases, primaryTargetId, "
         << "autoschedule) ";

    cout << "VALUES (" 
	 << targetId << ", "
	 << "'" << catalog << "', "
	 << raHours << ", "
	 << decDeg << ", "
	 << mura << ", "
	 << mudec << ", "
	 << parallax << ", "
	 << "'" << spectralType << "', "
	 << bMag << ", "
	 << vMag << ", "
	 << "'" << targetName << "', "
         << primaryTargetId << ", "
	 << "'" << autoschedule << "'"
	 << ");" << endl;
}
