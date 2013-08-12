#!/bin/tcsh

# This scripts cleans up any orphan channelizers and dxs
# on the signal processing machines.

set me=`whoami`

foreach h ( seti000-2 seti001-1 seti002-1 )

set chanCmd="sudo pkill channelizer"

echo "ssh ${me}@$h $chanCmd"
ssh ${me}@$h $chanCmd

set chanCmd1="cd sonata_install/bin;sudo rm 'core*'"

echo "ssh ${me}@$h $chanCmd1"
ssh ${me}@$h $chanCmd1

end

foreach h (  seti000-1 seti000-3 seti000-4 seti001-2 seti001-3 seti001-4 seti002-2 seti002-3 seti002-4 )

set dxCmd="sudo pkill dx"

echo "ssh ${me}@$h $dxCmd"
ssh ${me}@$h $dxCmd

end
