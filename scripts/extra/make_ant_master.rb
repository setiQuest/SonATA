#!/usr/bin/env ruby

###############################################################################
#
# File:    make_ant_master.rb
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
#################################################################################
#
# Creates the antenna-list-master.tcl from a list of antenna pols.
# See the help print out below for more detail.
#
require 'rubygems'
require 'time'
require 'date'


#################################
# Class to manage antenna lists #
#
# Example antenna-list-master.tcl:
#
# tscope set antsprimary 1a,1c,1d,1e,1g,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,5g
# tscope set antsxpol 1a,1c,1d,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,5g
# tscope set antsypol 1a,1c,1d,1e,1g,2b,2c,2e,2f,2g,2j,3d,3l,4k,4l,5b,5c,5g
#
# # ANTS MARKED GOOD (BF1)
# tscope assign beamxc1 1a,1c,1d,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,5g
# tscope assign beamyc1 1a,1c,1d,1e,1g,2b,2c,2e,2f,2g,2j,3d,3l,4k,4l,5b,5c,5g
#
# # ANTS MARKED GOOD (BF2)
# tscope assign beamxd1 1a,1c,1d,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,5g
# tscope assign beamyd1 1a,1c,1d,1e,1g,2b,2c,2e,2f,2g,2j,3d,3l,4k,4l,5b,5c,5g
#
# # ANTS MARKED GOOD (BF3)
# tscope assign beamxd2 1a,1c,1d,2b,2c,2e,2f,2g,2j,2k,2m,3d,3j,3l,4e,4k,4l,5b,5c,5g
# tscope assign beamyd2 1a,1c,1d,1e,1g,2b,2c,2e,2f,2g,2j,3d,3l,4k,4l,5b,5c,5g
#
#################################
class AntList

	def self.createMasterList(antList, listToRemove)

		full = getFullAntList(antList);
		xpol = self.getAntlistForPol(antList, "x");
		ypol = self.getAntlistForPol(antList, "y");

		if(listToRemove != nil)
			full = remove(listToRemove, full);
			xpol = remove(listToRemove, xpol);
			ypol = remove(listToRemove, ypol);
		end

		puts "\n";
		puts "tscope set antsprimary " + full + "\n";
		puts "tscope set antsxpol " + xpol + "\n";
		puts "tscope set antsypol " + ypol + "\n";
		puts "\n";
                puts "# ANTS MARKED GOOD (BF1)\n";
		puts "tscope assign beamxc1 " + xpol + "\n";
		puts "tscope assign beamyc1 " + ypol + "\n";
		puts "\n";
                puts "# ANTS MARKED GOOD (BF2)\n";
		puts "tscope assign beamxd1 " + xpol + "\n";
		puts "tscope assign beamyd1 " + ypol + "\n";
		puts "\n";
                puts "# ANTS MARKED GOOD (BF3)\n";
		puts "tscope assign beamxd2 " + xpol + "\n";
		puts "tscope assign beamyd2 " + ypol + "\n";
		puts "\n";

	end

	def self.getAntlistForPol(antList, pol)
	   parts = antList.split(",");
	   new_list = "";
	   parts.each do |ant_with_pol|
		   if(ant_with_pol.index(pol) != nil)
			   new_list += ant_with_pol.chop;
			   new_list += ",";
		   end
	   end
	   return new_list.chop;
	end

	def self.getFullAntList(antList)
	   parts = antList.split(",");
	   new_list = "";
	   parts.each do |ant_with_pol|
		   ant_without_pol = ant_with_pol.chop;

		   #puts ant_without_pol + ":" + ant_with_pol + "\n";

		   if(new_list.index(ant_without_pol) == nil)
			   new_list += ant_without_pol;
			   new_list += ",";
		   end
	   end
	   return new_list.chop;
	end

	def self.remove(antListToRemove, masterList)
		if(antListToRemove == nil)
			return masterList;
		end
		antListToRemove = antListToRemove.split(",");

		newList = masterList;
		antListToRemove.each do |ant|
			newList = newList.gsub(ant, "");
		end
		newList = newList.gsub(",,", ",").gsub(",,", "");
		if(newList.end_with?(","))
			newList = newList.chop;
		end

		if(newList.start_with?(",") == true)
			newList = newList.sub(",", "");
		end

		return newList;
	end



end

if(ARGV == nil || ARGV.size == 0)

	puts "\nThis program converts an antenna pol list into the format required for antenna-list-master.tcl.\n";
	puts "Arguments:\n";
	puts "  <antenna pol list> [antenna list to remove]\n";
	puts "Example:\n";
	puts "  make_ant_master.rb 1ax,1ay,1cx,1cy,1dx,1dy,1ey,...,5gx,5gy 1a,5g (will remove 1a, 5g from any list)\n"
	puts "  make_ant_master.rb 1ax,1ay,1cx,1cy,1dx,1dy,1ey,...,5gx,5gy (will not remove any ants from any list)\n"
	puts "\n\n";
	exit(1);
end


listToRemove = nil;
if(ARGV.size > 1)
	listToRemove = ARGV[1];
end

AntList.createMasterList(ARGV[0], listToRemove);

