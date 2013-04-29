#
################################################################################
#
# File:    README.txt
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

Notes about the scripts/extras directory

This directory contains simple scripts that aid the SonATA operator. These
scripts help monitor and control SonATA.

In operation the following conitions are required:

The contents of this directory must reside in ~/scripts

  cd ~
  mkdir scripts
  cd scripts
  cp ~/SonATA/scripts/extra/* .

Also, the weather_watcher.rb script writes out its log to ~/scripts/log.

  mkdir ~/scripts/log

