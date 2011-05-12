#!/bin/tcsh

cd ${HOME}/SonATA/sse-pkg

./reconfig

make -j25 install

cd ${HOME}/SonATA/scripts

./reconfig

make -j25 install
