#!/usr/bin/env ruby
#
require 'rubygems'
require 'time'
require 'date'

# Change these global values accordingly
$maxWindSpeed = 29;
$toEmailAddress  = 'observing@seti.org'
$fromEmailAddress = 'observing@seti.org'
$antListMasterFile = '/home/sonata/sonata_install/scripts/antenna-list-master.tcl';
$antListUse = '/home/sonata/sonata_install/scripts/antenna-list.tcl';
$fromAntGroup = 'none';

#################################
# Class to manage antenna lists #
#################################
class AntList

	# Get the primary antenna list from antenna-list.tcl
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

	def self.getUnion(groupName, origAntList)

		cmd = '/home/sonata/sonata_install/bin/backendCommand antunion ' + groupName + ' ' + origAntList;
		result = `#{cmd}`;
		if(result.index("rror") != nil)
			return "";
		end

		return result
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
			else
				f.write(line + "\n");
			end
		end

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
		cmd = 'echo "' + message + '" | mailx -r ' + fromAddr + ' -s "' + subject + '" ' +  toAddr;
		puts cmd;
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
			puts line + "\n";
			if(line.index("WindSpeedMax") != nil)
				parts = line.split;
				return parts[3];
			end
		end

		return "-1";
	end

	#determine if the wind speed is valid. 
	def isValid()
		if(getMaxWindSpeedMph().equals?("-1"))
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
##If the wind speed is too high, send email and exit.
weather = Weather.new($maxWindSpeed);
if(weather.isTooHigh())
	subject = "SonATA Observation start aborted.";
	message  = DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
	message += "The SonATA observation has been aborted.\n";
	message += "Wind gusts are too high at " + weather.getMaxWindSpeedMph() + "mph.";
        Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	exit;
end

#Get the master list of antennas to use
masterAntList = AntList.getOrigAntList();
#if the master list id bad - this is an error - report it and exit
if(masterAntList.length < 3)
	subject = "SonATA Observation start aborted.";
	message  = DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
	message += "The master list of antennas read from " + $antListMasterFile +  " does not contain valid information. Please check this file. The wind speed was " + weather.getMaxWindSpeedMph() + "mph, which is good, though!\nSo, nice try!";
        Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	exit;
end

#Get the union of the master list and the none group
unionAntList = AntList.getUnion($fromAntGroup, masterAntList);
if(unionAntList.length < 3 || unionAntList.index("empty") != nil)
	subject = "SonATA Observation start aborted.";
	message  = DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n\n";
	message += "The antlist that is the union of " + masterAntList + " and the " + $fromAntGroup + " group is not valid. ";
	message += "Here are the reported antenna groups from fxconf.rb:\n\n";
	message += `/home/sonata/sonata_install/bin/backendCommand fxconf.rb sals` + "\n";
        message += "The wind speed was " + weather.getMaxWindSpeedMph() + "mph, which is good, though!\nSo, nice try!";
        Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	exit;
end

#If this gets this far - execute the command...
command = ""
ARGV.each do |a|
	  command +=  a + " ";
end

# Wrtie out the ant list to use
AntList.writeUseAntsList(unionAntList);

subject = "SonATA Observation started.";
message  = "SonATA observing has started. :)\n\n";
message += 'Executing \"' + command + '\"';
message += "\n\n";
message += DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
message += "\nHere are the reported antenna groups from fxconf.rb:\n\n";
message += `/home/sonata/sonata_install/bin/backendCommand fxconf.rb sals` + "";
message += "\nThe antennas to be used are " + unionAntList + "\n\n";
message += "The wind speed was " + weather.getMaxWindSpeedMph() + "mph, which is good for observing.\n";


Email.send($toEmailAddress, subject, $fromEmailAddress, message);

puts `#{command}`
