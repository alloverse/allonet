#!/bin/sh
set -ex
VERSION=`cat build/include/allonet_version.txt | xargs echo -n`
STAGE=$1
PLATFORM=$2

if [ -z "$STAGE/version" ];
then
    echo "no build found to rename"
    exit 1
fi

mv "$STAGE/version/build" "$STAGE/version/$PLATFORM"
mv "$STAGE/version" "$STAGE/$VERSION"
