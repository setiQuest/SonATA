################################################################################
#
# File:    permRfiMask.tcl
# Project: OpenSonATA
# Authors: The OpenSonATA code is the result of many programmers
#          over many years
#
# Copyright 2011 The SETI Institute
#
# OpenSonATA is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OpenSonATA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
# 
# Implementers of this code are requested to include the caption
# "Licensed through SETI" with a link to setiQuest.org.
# 
# For alternate licensing arrangements, please contact
# The SETI Institute at www.seti.org or setiquest.org. 
#
################################################################################

# NSS Permanent RFI Mask for Hat Creek
# Values generated on Dec 2006 using Ants 3dY, 3eY.
# Preliminary cutoff selection by Peter Backus.
# Note: all values are in MHz

# band center freq & width
set bandcovered  { 6000.00 12000.0 }

# mask elements
# center freq & width


# Center  	Width  	Min  	        Max
# 1542.613 	44.045 	1520.591 	1564.636
# 1575.285 	8.192 	1571.189 	1579.381
# 1584.706 	0.819 	1584.296 	1585.116
# 1599.548 	12.480 	1593.307 	1605.788
# 1681.153 	1.638 	1680.334 	1681.972
# 1684.840 	0.819 	1684.430 	1685.249
# 1689.461 	5.146 	1686.887 	1692.034

# Total  	73.140 MHz 	 	

set masks  {
1542.613  	44.04
1575.285 	8.192
1584.706 	0.819
1599.548 	12.480
1681.153 	1.638
1684.840 	0.819
1689.461 	5.146
3040.4096 .8192
3041.2288 .8192
3046.144 .8192
3036.9632 .8192
3049.4208 .8192
3053.5168 .8192
3054.336 .8192
3057.6128 .8192
3058.432 .8192
3124.7861 .8192
3215.716 .8192
3216.535 .8192
3221.450 .8192
3222.269 .8192
3223.089 .8192
3223.908 .8192
}
