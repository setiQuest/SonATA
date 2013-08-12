#!/bin/sh

# sonata-cron-begin-exoplanets-session

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
${HOME}/sonata_install/bin/sonata-startup-cron-wrapper.sh -batch

# allow enough time for all components to be ready
sleep 120

#send command to SSE to start Observing
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-beam1-beam3-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beam-8500-8700-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-2beams-beam1-beam2-8500-8700-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-2beams-beam1-beam2-2840-3040-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-2beams-beam1-beam2-1400-1750-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-1400-1750-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-1152-1352-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-3800-4200-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-6567-6767-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-6767-6967-obs.tcl
#DONE uncomment the line below and comment the line above
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-6967-7167-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8920-9000-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8840-8920-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8760-8840-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8680-8760-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8600-8680-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8520-8600-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8440-8520-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8360-8440-obs.tcl
#${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8280-8360-obs.tcl
${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-8200-8280-obs.tcl
