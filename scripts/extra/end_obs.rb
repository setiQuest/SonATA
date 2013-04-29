#!/usr/bin/env ruby

###############################################################################
#
# File:    end_obs.rb
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
#
# End the curently running observation.
#
require 'rubygems'
require 'time'
require 'date'

# Change these global values accordingly
$toEmailAddress  = 'observing@seti.org';
$fromEmailAddress = 'observing@seti.org';
$fromAntGroup = 'bfa';
$toAntGroup = 'none';
$test = false;

#################################
# Class to manage antenna lists #
#################################
class Ants

	# Get the primary antenna list from antenna-list.tcl
	# tscope set antsxpol 1a,1c,1d,1k,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,5g
	# tscope set antsypol 1a,1c,1d,1e,1g,1k,2b,2c,2e,2f,2g,2j,3d,3l,4k,4l,5b,5c,5g
	#
	def self.getFxconfListForGroup(groupName)

		antGroups = `/home/sonata/sonata_install/bin/backendCommand fxconf.rb sals`

		# Get the antennas in the "groupName" group
		antGroups.each do |line|
			line = line.chomp;
			if(line.start_with?(groupName) == true)
				len = groupName.length;
				return line[len..1000].strip;
			end
		end

		return "";
	end

	def self.moveAntsToGroup(antList, toGroup)

		cmd = "/home/sonata/sonata_install/bin/backendCommand fxconf.rb satake " + toGroup + " " + antList;
		`#{cmd}`
	end

end

#######################
# Class to send email #
#######################
class Email

	#send email
	# mailx -s 'SonATA taking array for Rosetta Test' -r jjordan@seti.org ata-staff@seti.org
	#mailx: option requires an argument -- h
	#Usage: mailx [-BDFintv~] [-s subject] [-a attachment ] [-c cc-addr] [-b bcc-addr]
	#            [-r from-addr] [-h hops] [-A account] [-R reply-addr] [-S option] to-addr ...
	#            mailx [-BDeHiInNRv~] [-T name] [-A account] -f [name] [-S option]
	#            mailx [-BDeinNRv~] [-A account] [-u user] [-S option]
	def self.send(fromAddr, subject, toAddr, message)

		instructions = "\n";

		cmd = 'echo "' + message + instructions + '" | mailx -r ' + fromAddr + ' -s "' + subject + '" ' +  toAddr;
		`#{cmd}`
	end

end

#Sleep for 1  minute
sleep(60);

#get the list of ants in bfa group. there "should" not be any.
bfaAntList = Ants.getFxconfListForGroup($fromAntGroup);

# If there are antennas in the from group - need to put them into the none group.
if(bfaAntList.length > 0)
	Ants.moveAntsToGroup(bfaAntList, $toAntGroup);
	alarmCommand = "/home/sonata/sonata_install/bin/setAlarm ARM,sonata,SonATA is Finished."
        `#{alarmCommand}`
	Email.send($fromEmailAddress, "SonATA finished", $toEmailAddress, "WARNING: There were antennas left in bfa and the end_obs.rb script moved them to \"none\" and set the alarm message. SonATA must have crashed?");
end

exit 0

