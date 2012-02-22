################################################################################
#
# File:    rcvrBirdieMask.tcl
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


# ATA/NSS Receiver birdies
# Each mask entry is a relative offset in MHz from the
# receiver RF sky freq.
# All values are in MHz.

# Scanned both beamformers on 26 Nov 2008.
# Bandwidth was 30 MHz, covering +4 to +34Mhz relative to baseband left edge.
# (RF tune offset of -19 Mhz, dx center to baseband center offset +6 Mhz)
# Looked at only ant 1g.
# No significant birdies were found.

# Subsequent RFI scans showed a few extra low power birdies, which were added.
# Note: this seems to be very dependent on which ants are in use

# Format:
# set bandcovered { <centerfreqMhz> <widthMhz> }
# set masks {
# <freq1Mhz> <width1Mhz>
# <freq2Mhz> <width2Mhz>
# ...
# }

set bandcovered {0.0 45}
set masks {
-15.1606      .000533
-15.1670      .000533
-9.7154       .000533
-9.7155      .000533
-9.7186       .000533
-9.7187      .000533
-1.0923 .000533
-0.5536      .000533
-0.4832      .000533
-0.4768      .000533
-0.4704      .000533
-0.4640      .000533
-0.4576      .000533
-0.4512      .000533
-0.4448      .000533
-0.4384      .000533
-0.4320      .000533
-0.4256      .000533
-0.4192      .000533
-0.4128      .000533
0.0           .8192
1.0923 .000533
}
