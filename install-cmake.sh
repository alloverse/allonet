#!/bin/sh
set -x
version='3.15.4'

prefix=/usr/local
if [ "$1" != "" ]; then
    prefix=$1
fi

if [ "$prefix" = "/usr/local" ]; then
    rm -rf /usr/local/bin/cmake*
    rm -rf /usr/local/bin/ctest*
    rm -rf /usr/local/doc/cmake
    rm -rf /usr/local/man/man1/cmake*
    rm -rf /usr/local/man/man1/ctest*
    rm -rf /usr/local/man/man7/cmake*
    rm -rf /usr/local/share/cmake*
else
    mkdir -p $prefix
fi

wget -O cmake-linux.sh https://cmake.org/files/v3.13/cmake-${version}-Linux-x86_64.sh
sh cmake-linux.sh -- --skip-license --prefix=$prefix
$prefix/bin/cmake --version