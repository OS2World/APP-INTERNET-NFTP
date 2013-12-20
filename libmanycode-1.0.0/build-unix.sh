#!/bin/sh

if [ ! -f build-unix.sh ] 
then
   echo "This script must be run from its own directory"
   exit
fi

PLACE=`pwd`

C_INCLUDE_PATH=$PLACE/../include:$C_INCLUDE_PATH;
LIBRARY_PATH=$PLACE/../lib:$LIBRARY_PATH;
export C_INCLUDE_PATH
export LIBRARY_PATH

sh configure --prefix=$PLACE/.. --enable-shared=no
cd src
make all install
