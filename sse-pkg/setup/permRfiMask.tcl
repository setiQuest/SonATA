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

set masks  {
1521.819 .8192
1525.915 .8192
1529.192 .8192
1531.650 .8192
1533.288 .8192
1534.107 .8192
1535.746 .8192
1537.384 .8192
1539.842 .8192
1540.661 .8192
1543.119 .8192
1546.395 .8192
1548.853 .8192
1551.119 .8192
1551.938 .8192
1554.396 .8192
1556.835 .8192
1557.864 .8192
1560.130 .8192
1560.949 .8192
1561.769 .8192
1562.558 .8192
1563.407 .8192
1564.226 .8192
1571.790 .8192
1572.609 .8192
1573.429 .8192
1574.247 .8192
1575.067 .8192
1575.886 .8192
1576.705 .8192
1577.524 .8192
1578.343 .8192
1597.184 .8192
1598.004 .8192
1598.823 .8192
1599.642 .8192
1600.462 .8192
1601.281 .8192
1602.100 .8192
1602.919 .8192
1603.738 .8192
1604.557 .8192
1605.378 .8192
1681.561 .8192
1690.573 .8192
1691.391 .8192
1691.624 .8192
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
