################################################################################
#
# File:    Makefile.am
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


# Override the default LIBS setting so that
# we don't pick up any dependencies on tcl etc.
# that are not needed.

LDADD=

bin_SCRIPTS = \
	email-jpl-ephem-xyz-requests.sh\
	convert-jpl-xyz-data.sh \
	convert-jpl-horiz-to-ata-ephem.sh \
	compareDxData.sh \
	controlcomponents.expect \
	daily-error-summary.sh \
	haltmachines.tcl \
	deleteOldSseArchiveData.sh \
	hat-creek-ntp-server-ping-test.sh \
	ssemailmsg.sh \
	sonata-extract-cal-info.sh \
	sonata-extract-confirmed-sigs.sh \
	sonata-extract-observers-log-info.sh \
	sonata-extract-systemlog-info.sh \
	extract-delay-cals.sh \
	current-system-log.sh \
	watch-confirmed-sigs.sh \
	watch-for-observers-log-info.sh \
	runsse.sh \
	runsse-test-screen.sh \
	startssedd.sh \
	startbaselines.sh \
	startwaterfalls.sh \
	seekerlib.tcl \
	sse-watcher.sh \
	obs-summary-report.sh \
	obs-cal-report.sh \
	obs-delay-cal-report.sh \
	verifySonATATestSignal.py \
	pushsshbintail.sh \
	send-seeker-command-via-telnet.expect \
	sonata-shutdown-cron-wrapper.sh \
	sonata-startup-cron-wrapper.sh \
	sonata-seeker-command-cron-wrapper.sh \
	run-command-with-timeout.expect \
	createPermRfiMaskElements.sh \
	clear-archive-disk.sh \
	backup-db-by-date.sh \
	delete-old-backup-db-by-date-files.sh \
	list-prelude-files-for-backup.sh \
	backup-prelude-files-to-tape.sh \
	backup-slave-db.sh \
	dump-all-mysqldb.sh \
	run-sonata-prog-under-cron.sh \
	compare-dir-checksums.sh \
	create-starmap.sh \
	create-observing-starmap.sh \
	create-observing-data-images.sh \
	send-obs-info-to-webserver.sh \
	create-observing-targetmap.sh \
	mybackup.sh \
	db-backup.sh \
	db-backup-sse100.sh \
	sse100-backup.sh \
	sse300-backup.sh \
	db-backup.sse300.sh \
	sonata-startup-cron-vger-wrapper.sh \
	setilive-summary.sh \
	opensonata-build-test.sh 

bin_PROGRAMS = \
	fileModTimeInSecs \
	currentTimeInSecs \
	bintail \
    systemStatusToCurses \
    setAlarm \
    backendCommand

fileModTimeInSecs_SOURCES = \
	fileModTimeInSecs.cpp

currentTimeInSecs_SOURCES = \
	currentTimeInSecs.cpp

bintail_SOURCES = \
	bintail.cpp

systemStatusToCurses_SOURCES = \
	systemStatusToCurses.cpp

setAlarm_SOURCES = \
	setAlarm.cpp

backendCommand_SOURCES = \
	backendCommand.cpp

# cause the scripts to get distributed and
# installed when "make dist" is used

EXTRA_DIST = $(bin_SCRIPTS)
#install_SCRIPTS = $(bin_SCRIPTS)
