/*******************************************************************************

 File:    readephem.h
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

/*	readephem.h
	declarations for routines in readephem.c
 *
 *	$log$
95 jan 16 JWD	coded basic input stuff.  packing for use awaits.
95 jan 18 JWD	working on interpolator.  position() now retunrs status
95 jan 22 JWD	improved interpolator.  adding acceleration calc.

*/

#define LINESINHEADER   9       /* number of lines to skip over */
#define MAXCHARS        900     /* more chars than reasonable in header */
#define ARGSINROW       10       /* numbers in each row of input table */
#define MAXROWS         800     /* more rows than reasonable in a table */
#define STATE_VECT_LEN 12 /* x y z vx vy vz ax ay az lt range rangerate */

long skipheader( FILE *ephemfile );
int readxyztable( FILE *ephemfile, double tbl[][ARGSINROW] );
void printxyztable( int n_rows, double tbl[][ARGSINROW] );
int position( char *filename, long jday, double jsec, double statevect[STATE_VECT_LEN] );

