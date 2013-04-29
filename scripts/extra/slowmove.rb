#!/usr/bin/env ruby

###############################################################################
#
# File:    antgroups
# Project: OpenSonATA
# Authors: Jon Richards, SETI Institute
#
# Copyright 2013 The SETI Institute
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

# Slowly move the antennas. You can change the parameter for timing, az, el.

require 'rubygems'
require 'time'
require 'date'

antlist = '1a,1c,1d,1e,1g,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,3f,3g,3h,1b,1f,1h,1k,2a,3e,4f,4g'
startaz = 30;
endaz = 330;
el = 40;

duration = 1800; #seconds
iterationtime = 6; #seconds;
azstep = ((endaz.to_f - startaz.to_f) / duration.to_f) * iterationtime.to_f;

puts azstep.to_s + "\n";

i = 0;
az = startaz;
until az > endaz

	cmd = 'backendCommand atasetazel ' + antlist + ' ' + az.to_s + ' ' + el.to_s;
        `#{cmd}`
	puts cmd + "\n";
	az = az + azstep;
	puts az.to_s + "," + startaz.to_s + "\n";
	sleep(iterationtime);

end
