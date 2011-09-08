#!/bin/tcsh

foreach h ( seti003-2: seti003-3: seti003-4: )

set me=`whoami`
cd
echo "copying sonata_install to $h"
scp -rp sonata_install ${me}@$h/home/${me}/
end
