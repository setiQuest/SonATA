#!/bin/tcsh

cd ${HOME}/SonATA/sse-pkg

./reconfig --dxlibsonly

make -j13 install

cd ${HOME}/SonATA/sig-pkg

./reconfig

make -j13 install

cd ${HOME}/SonATA/scripts

make install
