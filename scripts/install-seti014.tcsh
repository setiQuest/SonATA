#!/bin/tcsh

foreach h ( seti014-2: seti014-3: seti014-4: )

set me=`whoami`
cd
echo "copying sonata_install to $h"
scp -rp sonata_install ${me}@$h/home/${me}/
end
