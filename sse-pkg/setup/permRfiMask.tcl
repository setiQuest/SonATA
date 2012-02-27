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

#1525.7456	.8192
#1527.3856	.8192
#1531.4816	.8192
# Total  	73.140 MHz 	 	

set masks  {
1533.9392	.8192
1535.5776	.8192
1537.2160	.8192
1538.8544	.8192
1539.6736	.8192
1540.4912	.8192
1541.3104	.8192
1542.1296	.8192
1546.2272	.8192
1547.0464	.8192
1551.1424	.8192
1551.9616	.8192
1555.2384	.8192
1556.0576	.8192
1557.6944	.8192
1558.5136	.8192
1565.0000	4.800
1571.6205	.8192
1572.2589	.8192
1573.2589	.8192
1574.0800	.8192
1574.8992	.8192
1575.7168	.8192
1576.5375	.8192
1577.3576	.8192
1578.1760	.8192
1598.6550	.8192
1600.1136	.8192
1601.9328	.8192
1603.5712	.8192
1604.3804	.8192
1681.3920	.8192
1684.6688	.8192
1687.1264	.8192
}
#1605.2096	.8192
#1691.2221	.8192
#1597.8368	.8192
