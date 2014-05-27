#!/bin/bash
#
PRG=$1
if [ -z $PRG ]; then
PWD=`pwd`
PRG=`basename $PWD`
fi

# for ez430 programmer
#mspdebug -d /dev/ttyUSB0 uif "prog $PRG.elf"
# for rf2500 / launchpad programmer
mspdebug rf2500 "prog $PRG.elf"
