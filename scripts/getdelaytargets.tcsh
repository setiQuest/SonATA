#!/bin/tcsh

# getdelaytargets.tcsh

findtargets -dbhost sse100 -dbname sonatadb -tzoffset -7 -type caldelay
