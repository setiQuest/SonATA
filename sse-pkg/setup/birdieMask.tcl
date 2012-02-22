################################################################################
#
# File:    birdieMask.tcl
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


# BaseBand Birdies
# All values in MHz

# Format:
# set bandcovered { <centerfreq> <width> }
# set masks {
# <freq1> <width1>
# <freq2> <width2>
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
  0.0 		.8192
}
