#!/bin/tcsh

foreach h ( seti000-2: seti000-3: seti000-4: seti001-1: seti001-2: seti001-3: seti001-4: seti002-1: seti002-2: seti002-3: seti002-4: seti003-1 seti003-2 seti003-3 seti003-4 )

set me=`whoami`
cd
echo "copying sonata_install to $h"
scp -rp sonata_install ${me}@$h/home/${me}/
end
