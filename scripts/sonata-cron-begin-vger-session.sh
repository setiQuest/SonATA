#!/bin/sh

# sonata-cron-begin-vger-session.sh

#run the test to see if we should be running. If not - exit the script.
/home/sonata/scripts/should_start_obs.rb
if [ $? -ne 0 ]; then
  exit
fi
#Start the backend process that watches the weather
/home/sonata/scripts/weather_watch.rb &
# Kill all the dxs and channelizers in case they weren't shutdown properly
/home/sonata/sonata_install/scripts/cleanup-chan-dx.tcsh

# start a SonATA observing session
# start up SSE
${HOME}/sonata_install/bin/sonata-startup-cron-vger-wrapper.sh -batch

# allow enough time for all components to be ready
sleep 120

#send command to SSE to start Observing
${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-vger-2beams-8400-8440-obs.tcl
