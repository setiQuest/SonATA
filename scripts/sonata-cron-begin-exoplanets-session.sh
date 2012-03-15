#!/bin/sh

# sonata-cron-begin-exoplanets-session

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
${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-start-exoplanets-3beams-1400-1750-obs.tcl
