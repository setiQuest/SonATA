#!/bin/tcsh

cd ${HOME}/SonATA/sse-pkg

./reconfig --dxlibsonly

make -j3 install

cd ${HOME}/SonATA/sig-pkg

./reconfig

make -j3 install

cd ${HOME}/SonATA/scripts

make install
