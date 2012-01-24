#!/bin/tcsh

cd ${HOME}/SonATA/sse-pkg

./reconfig --dxlibsonly

make -j25 install

cd ${HOME}/SonATA/sig-pkg

./reconfig

make -j25 install

cd ${HOME}/SonATA/scripts

./reconfig
make install
