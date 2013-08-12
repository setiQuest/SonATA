#!/usr/bin/env ruby
#
###############################################################################
#
# File:    should_start_obs.rb
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

# This script is run at the start of every observation. It checks to see if there
# are antennas in the none grup and if the winds a calm enough.
#
#
require 'rubygems'
require 'time'
require 'date'

# Change these global values accordingly
$maxWindSpeed = 29;
$toEmailAddress  = 'observing@seti.org';
$fromEmailAddress = 'observing@seti.org';
$antListMasterFile = '/home/sonata/sonata_install/scripts/antenna-list-master.tcl';
#$toEmailAddress  = 'jrichards@seti.org';
#$fromEmailAddress = 'jrichards@seti.org';
#$antListMasterFile = '/home/sonata/sonata_install/scripts/antenna-list-master-test.tcl';
#$antListUse = '/home/sonata/sonata_install/scripts/antenna-list-testonly.tcl';
$antListUse = '/home/sonata/sonata_install/scripts/antenna-list.tcl';
$fromAntGroup = 'none';
$test = false;

#################################
# Class to manage antenna lists #
#################################
class AntList

	# Get the primary antenna list from antenna-list.tcl
	# tscope set antsxpol 1a,1c,1d,1k,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,5g
	# tscope set antsypol 1a,1c,1d,1e,1g,1k,2b,2c,2e,2f,2g,2j,3d,3l,4k,4l,5b,5c,5g
	#
	def self.getOrigAntList

		cmd = 'cat ' + $antListMasterFile;
		origAntList = `#{cmd}`

		# Find tscope set antsprimary
		origAntList.each do |line|
			line = line.chomp;
			if(line.index("tscope set antsprimary") != nil)
				parts = line.split;
				return parts[3];
			end
		end

		return "";
	end

	def self.getUseAntListFileContents
		cmd = 'cat ' + $antListUse;
		return  `#{cmd}`
	end

	def self.getUnion(groupName, origAntList)

		cmd = '/home/sonata/sonata_install/bin/backendCommand antunion ' + groupName + ' ' + origAntList;
		result = `#{cmd}`;
		if(result.index("rror") != nil)
			return "";
		end

		return result
	end

	def self.unionOf2Lists(list1, list2)

		ants1 = list1.split(","); 
		ants2 = list2.split(","); 

		newlist = "";

		ants1.each do |a1|
                  ants2.each do |a2|
			  if(a1.chomp.eql?(a2.chomp))
				 newlist += a2.chomp;
				 newlist += ",";
			  end
		  end
		end

		newlist.chomp!(",");

		return newlist;
	end

	def self.remove(doesnotexist, list)
		doesNotExistList = doesnotexist.split(",");
		if(doesnotexist == nil) 
			return list;
		end

		newList = list;
		doesNotExistList.each do |ant|
			newList = newList.gsub(ant, "");
		end
		newList = newList.gsub(",,", ",").gsub(",,", "");
		if(newList.end_with?(","))
			newList = newList.chop;
		end
		return newList;
	end

	def self.checkWhichAntsDoNotExist(list)

		ants = list.split(",");
		if(ants == nil)
			return nil;
		end

		badlist = "";

		ants.each do |ant|
			cmd = "/home/sonata/sonata_install/bin/backendCommand atagetazel " + ant;
			result = `#{cmd}`;
			#puts cmd + "\n";
			puts result + "\n";
			if(result.index('ant') == nil)
				badlist += ant + ",";
			end
		end

		if(badlist.length > 0) 
			badlist = badlist.chop;
		else
			return nil;
		end

		return badlist;
	end

	def self.writeUseAntsList(list)

		f = File.open($antListUse, 'w');

		cmd = 'cat ' + $antListMasterFile;
		origAntList = `#{cmd}`

		# Find tscope set antsprimary
		origAntList.each do |line|
			line = line.chomp;
			if(line.index("tscope set antsprimary") != nil)
				f.write("tscope set antsprimary " + list );
			elsif(line.start_with?("tscope set") == true)
				parts = line.split(" ");
				newlist = unionOf2Lists(list, parts[3]);
				newLine = parts[0] + " " + parts[1] + " " + parts[2] + " " + 
					newlist + "\n";
#					getUnion($fromAntGroup, parts[3]);
				f.write(newLine);
			elsif(line.start_with?("tscope assign") == true)
				parts = line.split(" ");
				newlist = unionOf2Lists(list, parts[3]);
				newLine = parts[0] + " " + parts[1] + " " + parts[2] + " " + 
					newlist;
#					getUnion($fromAntGroup, parts[3]);
				f.write(newLine + "\n");
			else
				f.write(line + "\n");
			end
		end
		f.close;

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

		instructions  = "\n****************************************************\n";
		instructions += "\nIf in doubt as to why this message was received here is an explanation of what the script does:\n";
		instructions += "1) Gets the list of desired antennas out of antenna-list-master.tcl. This is 'lista'.\n";
		instructions += "2) Gets the list of antennas in the '" + $fromAntGroup + "' group. This is 'listb'.\n";
		instructions += "3) A new list is created listc = lista intersection listb.\n";
		instructions += "4) Each antenna in listc is checked to see if it exists by issuing 'atagetazel <antenna name>'. If any are unresponsive they are removed from listc.\n";
		instructions += "5) If there are < 3 antennas in listc, this email is sent and obs aborted.\n";
		instructions += "6) antennal-list.tcl is created using listc.\n";
		instructions += "7) The wind speed gusts are checked. If >= 29mph, this email is sent and obs aborted.\n";
		instructions += "8) If it makes it this far this email is sent and the obs script can continue.\n";
		instructions += "\n\n";

		cmd = 'echo "' + message + instructions + '" | mailx -r ' + fromAddr + ' -s "' + subject + '" ' +  toAddr;
		#puts cmd;
		`#{cmd}`
	end

end

##############################
# Class to maage the weather #
##############################
class Weather

	attr_accessor :maxWind, :weatherReport;

	def initialize(maxWind)
		@maxWind = maxWind;
		@weatherReport = `/home/sonata/sonata_install/bin/backendCommand ataweather -l`;
	end

	#Read the weather report and get the max wind speed (gusts).
	def getMaxWindSpeedMph()

		# Find tscope set antsprimary
		@weatherReport.each do |line|
			line = line.chomp;
			#WindSpeedMax (mph) = 29.00
			#puts line + "\n";
			if(line.index("WindSpeedMax") != nil)
				parts = line.split;
				return parts[3];
			end
		end

		return "-1";
	end

	#determine if the wind speed is valid. 
	def isValid()
		if(getMaxWindSpeedMph().eql?("-1"))
			return false;
		else 
			return true;
		end
	end

	#create a String representation of the
	def to_s
		return @weatherReport;
	end

	# determine if the wind is too high
	def isTooHigh()

		if(getMaxWindSpeedMph().to_f >= @maxWind.to_f)
			return true;
		end
		return false;
	end


end


#Check the wind speed
##If the wind speed is too high, send email and exit(1).
weather = Weather.new($maxWindSpeed);
if(weather.isTooHigh())
	subject = "SonATA Observation start aborted. :(";
	message  = "SonATA Observation start aborted. :(\n\n";
	message += DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
	message += "The SonATA observation has been aborted.\n";
	message += "Wind gusts are too high at " + weather.getMaxWindSpeedMph() + "mph.\n";
	Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	exit(1);
end

#Get the master list of antennas to use
masterAntList = AntList.getOrigAntList();
#puts "MASTER: " + masterAntList + "\n";
#if the master list id bad - this is an error - report it and exit(1)
if(masterAntList.length < 3)
	subject = "SonATA Observation start aborted.";
	message  = "SonATA Observation start aborted. :(\n\n";
	message += DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
	message += "The master list of antennas read from " + $antListMasterFile +  " does not contain valid information. Please check this file. The wind speed was " + weather.getMaxWindSpeedMph() + "mph, which is good, though!\nSo, nice try!\n";
	Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	exit(1);
end

#Get the union of the master list and the none group
unionAntList = AntList.getUnion($fromAntGroup, masterAntList);
#Get a list of natennas that do not exist.
doesnotexist = AntList.checkWhichAntsDoNotExist(unionAntList);
#Remove any antennas that are not responsive.
#puts "UNION: " + unionAntList + "\n";
#puts "NOTEXIST: " + doesnotexist + "\n";
if(doesnotexist != nil)
	unionAntList = AntList.remove(doesnotexist, unionAntList);
end
#puts "UNION2: " + unionAntList + "\n";

#After all this, an error is determined by the list of antennas, which is a comma
#separated string, of < 10 antennas, or contains "empty".
#10 antennas = 30 characters. 10 x 2 characters each + 9 commas + a space at the end.
if(unionAntList.length < 30 || unionAntList.index("empty") != nil)
	subject = "SonATA Observation start aborted.";
	message  = "SonATA Observation start aborted. :(\n\n";
	message += DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n\n";
	message += "The antlist that is the union of " + masterAntList + " and the " + $fromAntGroup + " group is not valid. ";
	message += "Here are the reported antenna groups from fxconf.rb:\n\n";
	message += `/home/sonata/sonata_install/bin/backendCommand fxconf.rb sals` + "\n";
	if(doesnotexist != nil && doesnotexist.length > 2 && (doesnotexist.index("empty")  == nil))
		message += "\n!!! The following antennas are unresponsive and have been removed from the list: \n\n" + doesnotexist + "\n\n";
	end
	message += "The wind speed was " + weather.getMaxWindSpeedMph() + "mph, which is good, though!\nSo, nice try!\n";
	Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	exit(1);
end

#Check to see if any antennas do not exist.
#doesnotexist = AntList.checkWhichAntsDoNotExist(unionAntList);
#if(doesnotexist != nil)
#	subject = "SonATA Observation start aborted.";
#	message  = DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n\n";
#	message += "The following antennas requested do not exist!  \n\n" + doesnotexist + "\n";
#	message += "The master antenna list was: \n\n" + masterAntList + "\n\n";
#	message += "Here are the reported antenna groups from fxconf.rb:\n\n";
#	message += `/home/sonata/sonata_install/bin/backendCommand fxconf.rb sals` + "\n\n";
#	message += "The union of the master list and the 'none' group that would have been moved to 'bfa': \n\n" + unionAntList + "\n";
#	Email.send($toEmailAddress, subject, $fromEmailAddress, message);
#	exit(1);
#end


# Wrtie out the ant list to use
AntList.writeUseAntsList(unionAntList);

subject = "SonATA Observation can start.";
message  = "SonATA observing is cleared to start. :)\n\n";
message += "\n\n";
message += DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
message += "\nHere are the reported antenna groups from fxconf.rb:\n\n";
message += `/home/sonata/sonata_install/bin/backendCommand fxconf.rb sals` + "";
if(doesnotexist != nil)
	message += "\n!!! The following antennas are unresponsive and have been removed from the list: \n\n" + doesnotexist + "\n\n";
end
message += "\nThe antennas to be used are " + unionAntList + "\n\n";
message += "The wind speed was " + weather.getMaxWindSpeedMph() + "mph, which is good for observing.\n";

message += "\nThe resulting antenna-list.tcl:\n\n" +  AntList.getUseAntListFileContents + "\n\n";

# All is good - send the email and exit with a 0 status (success)
Email.send($toEmailAddress, subject, $fromEmailAddress, message);
#puts message;

#remove all the /tmp/setiquest-activity-time* files.
`rm /tmp/setiquest-activity-time*`
exit 0

