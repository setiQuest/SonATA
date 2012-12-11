#!/bin/sh

# sonata-cron-switch-to-exoplanets.sh

verbose level 2
# interrupt a SonATA observing session
${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-wrapup.tcl


# allow enough time for current strategy to finish
sleep 600

# change database and catalog, restart observing
${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-switch-to-exoplanets.tcl
