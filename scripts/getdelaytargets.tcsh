#!/bin/tcsh

# getdelaytargets.tcsh

findtargets -dbhost sse100 -dbname sonatadb -tzoffset -8 -type caldelay
