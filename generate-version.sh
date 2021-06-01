#!/bin/sh
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
OUT_DIR="$SCRIPT_DIR/src"
VERSION_H="$OUT_DIR/allonet_version.h"
VERSION_TXT="$OUT_DIR/allonet_version.txt"
PRODUCT="ALLONET"

mkdir -p "${OUT_DIR}"

VERSION=`git describe --abbrev=7 --long | sed "y/-/./"`
SHORTVERSION=`echo $VERSION | cut -f 1-3 -d "."`
MAJORVERSION=`echo $VERSION | cut -f 1 -d "."`
HASH=`echo $VERSION | cut -f 4 -d "."`

CONTENTS="#define ${PRODUCT}_VERSION $VERSION"$'\n'"#define ${PRODUCT}_NUMERIC_VERSION $SHORTVERSION"$'\n'"#define ${PRODUCT}_HASH $HASH"$'\n'"#define ${PRODUCT}_MAJOR_VERSION $MAJORVERSION"
if [ -e "$VERSION_H" ]
then
    EXISTING=`cat "$VERSION_H"`
fi

if [ "$EXISTING" = "$CONTENTS" ]
then
    echo "Version unchanged, still $VERSION"
    exit 0
fi

echo "Version changed: $VERSION"
echo "$CONTENTS" > "$VERSION_H"
echo "$VERSION" > "$VERSION_TXT"