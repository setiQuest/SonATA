#!/usr/bin/env ruby

###############################################################################
#
# File:    weather_watch.rb
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

# Watch the weather. If the winds get too high, windsock the antennas, send email.

require 'rubygems'
require 'time'
require 'date'

# Change these global values accordingly
$maxWindSpeed = 31;
$minWindSpeed = 25;
$toEmailAddress  = 'observing@seti.org'
$fromEmailAddress = 'observing@seti.org'
$wasTooHigh = false;

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

	#Read the weather report and get the wind direction
	def getWindDir()

		# Find tscope set antsprimary
		@weatherReport.each do |line|
			line = line.chomp;
			#WindDirectionAvg (deg) = 196.42
			#puts line + "\n";
			if(line.index("WindDirectionAvg") != nil)
				parts = line.split;
				return parts[3];
			end
		end

		return "-1";
	end

	#Read the weather report and calc the direction to windsock.
	def getWindsockDir()

		if(getWindDir().eql?("-1"))
			return -9999.to_s;
		end

		windDir = getWindDir().to_f;

		if(windDir > 180.0)
			windDir = windDir - 180.0;
		elsif
			windDir = windDir + 180.0;
		end

		return windDir.to_s;
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

	#create a short String representation of the
	def to_s_short
		return "WindSpeed = " + getMaxWindSpeedMph() + " mph, Dir=" + getWindDir() + " deg, sock dir=" + getWindsockDir() + " deg, valid=" + isWindsockValid().to_s;
	end

	#create a short String representation of the
	def alarmMessage

		result = "";

		if(isTooHigh())
		  result = "Gusts = " + getMaxWindSpeedMph() + " mph Windsocking to " + getWindsockDir() + "AZ 40EL";
		elsif
		  result = "Gusts = " + getMaxWindSpeedMph() + " mph. Resuming observations.";
		end

		return result;
	end

	def isWindsockValid()
		if(getWindsockDir().eql?("-9999"))
			return false;
		else
			return true;
		end
	end

	#Refresh the weather info
	def refresh
		@weatherReport = `/home/sonata/sonata_install/bin/backendCommand ataweather -l`;
	end

	# determine if the wind is too high
	def isTooHigh()

		#puts getMaxWindSpeedMph().to_s + "," + @maxWind.to_s + "\n";
		if(getMaxWindSpeedMph().to_f >= @maxWind.to_f)
			return true;
		end
		return false;
	end


end


def isInSched()

	sched = `wget -q -O - http://setiquest.info/feeds/sonataschedule.xml`;
	if(sched.index('current="yes"') != nil)
		return true;
	else
		return false;
	end
end

def isObserving()

	sched = `wget -q -O - http://setiquest.info/feeds/sonatastatus.xml`;
	if(sched.index('Observing') != nil)
		return true;
	else
		return false;
	end
end
$stdout = File.new( '/home/sonata/scripts/log/weather_watch.log', 'w' );
$stdout.sync = true;

#Create an instance of the weather
weather = Weather.new($maxWindSpeed);

# Loop forever till killed
loop do
	
	weather.refresh();
	#set these values to true for testing only
	#inSched = true;
	#observing = true;
	inSched = isInSched();
	observing = isObserving();
	puts "[" + DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "] " + weather.to_s_short + ", in schedule=" + inSched.to_s + ", isObserving=" + observing.to_s + "\n";
	if(inSched && ($wasTooHigh == false) && (weather.isTooHigh() == true))
		puts "[" + DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "] HIGH\n";
		$wasTooHigh = true;
		stopCommand = "/home/sonata/sonata_install/bin/send-seeker-command-via-telnet.expect stop";
		`#{stopCommand}`
		subject = "SonATA Observation stopped till winds die down.";
		message = "";
		message += DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
		message += "Wind gusts are too high at " + weather.getMaxWindSpeedMph() + "mph.\n";
		message += "Wind direction is " + weather.getWindDir() + " deg.\n";
		message += "The SonATA observation has been stopped with the following command:\n";
		message += stopCommand + "\n";
		if(weather.isWindsockValid())
			sockCommand = "/home/sonata/sonata_install/bin/backendCommand atasetazel all " + weather.getWindsockDir() + " 40";
			`#{sockCommand}`
			message += "Antennas were windsocked with the command:\n";
			message += sockCommand + "\n";
		elsif
			message += "ERROR! Antennas were NOT WINDSOCKED. There was an error retrieving the proper AZ to windsock.\n";
		end
		#Alarm
		alarmCommand = "/home/sonata/sonata_install/bin/setAlarm ARM,sonata," + weather.alarmMessage;
		`#{alarmCommand}`
		puts alarmCommand += "\n";
        	Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	elsif(inSched && (weather.isTooHigh() == true) && weather.isWindsockValid())
		#JR Dec 21, 2012 - Every minute if wind too high, adjust the windsock...
		sockCommand = "/home/sonata/sonata_install/bin/backendCommand atasetazel all " + weather.getWindsockDir() + " 40";
		`#{sockCommand}`
	elsif (inSched && ($wasTooHigh == true) && (weather.isTooHigh() == false) && (weather.getMaxWindSpeedMph().to_f <= $minWindSpeed))
		puts "[" + DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "] LOW\n";
		$wasTooHigh = false;
		startCommand = "/home/sonata/sonata_install/bin/send-seeker-command-via-telnet.expect start obs";
		if(observing == false)
			startCommand = "/home/sonata/sonata_install/bin/send-seeker-command-via-telnet.expect start tasks";
		end
		`#{startCommand}`
		#Alarm
		alarmCommand = "/home/sonata/sonata_install/bin/setAlarm ARM,sonata," + weather.alarmMessage;
		`#{alarmCommand}`
		puts alarmCommand += "\n";
		message = "";
		message  = DateTime.now().strftime('%b %d, %Y - %I:%M:%S %p GMT').to_s + "\n";
		message += "Wind gusts are " + weather.getMaxWindSpeedMph() + "mph.\n";
		message += "Wind direction is " + weather.getWindDir() + " deg.\n";
		subject = "SonATA Observation has been restarted.";
		message += "The SonATA observation has been started with the following command:\n";
		message += startCommand + "\n";
        	Email.send($toEmailAddress, subject, $fromEmailAddress, message);
	end
	sleep(60); # Sleep 1 minute
end


exit 0

