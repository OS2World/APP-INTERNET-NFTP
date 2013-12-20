#!/bin/sh

autoheader
rm aclocal.m4
libtoolize -c
aclocal
automake
#--gnu
autoconf

echo done creating autoconf files in `pwd`
