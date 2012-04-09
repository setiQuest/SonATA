#!/bin/sh

#Kill any instance of the weather watcher.
/home/sonata/scripts/weather_watch_kill.rb

# sonata-cron-end-obs-session.sh

# terminate a SonATA observing session

# send commands to SSE to stop observing
${HOME}/sonata_install/bin/sonata-seeker-command-cron-wrapper.sh source ${HOME}/sonata_install/scripts/sonata-stop-obs.tcl

# wait for system to wrap up
sleep 30

#shutdown sse
${HOME}/sonata_install/bin/sonata-shutdown-cron-wrapper.sh
