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
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

using namespace std;

void printDbTableLine(int targetId, double raHours, double decDeg,
                      int primaryTargetId, string autoschedule, string targetName);

void printHelp();

int main(int argc, char *argv[])
{

	if(argc != 7)
	{
		printHelp();
		return 0;
	}

	int targetId = atol(argv[1]);
	string targetName(argv[2]);
	double targetRaHours = atof(argv[3]);
	double targetDecDeg = atof(argv[4]);
	double ra = 0.0;
	double dec = 0.0;

	bool raIsInDegrees = false;
	if(!strcasecmp(argv[5], "true"))
		raIsInDegrees = true;

	// assign box center as primary pointing
	int primaryTargetId = -1;
	string autoschedule("No");
	printDbTableLine(targetId, targetRaHours, targetDecDeg, primaryTargetId,
			autoschedule, targetName);

	// all the others get this as there primary target id
	primaryTargetId = targetId++;

	autoschedule="Yes";

	//Open the file, read it line-by-line, creating the SQL statement.
	FILE *fp = fopen(argv[6], "r");
	if(!fp)
	{
		printf("Error: Can not open file %s\n", argv[6]);
	}
	char line [ 128 ]; /* or other suitable maximum line size */
	while ( fgets ( line, sizeof line, fp ) != NULL ) /* read a line */
	{
		sscanf(line, "%lf,%lf", &ra, &dec);
	        // If ra is in degrees, convert to hours
	        if(raIsInDegrees == true)
	        {
	          ra /= 15.0;
	        }

		//printf("%lf, %lf\n", ra, dec);
		printDbTableLine(targetId, ra, dec, primaryTargetId,
				autoschedule, targetName);
		targetId++;

	}

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

void printHelp()
{
	printf("Reads a comma separated line of RaDec values and outputs th TargetCat sql inserts.\n");
	printf("createCatFromFile: <target Id> <target name> <ra> <dec> <(is ra in degrees) true|false> <text file to read>\n");
	printf("Reads a file of ra,dec lines and outputs the SQL insert commands.\n");
}
