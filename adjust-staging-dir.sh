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

generate_metadata()
{
  cat <<EOF
{
  "version": "${VERSION}",
  "platform": "${PLATFORM}",
  "branch": "${BUILD_SOURCEBRANCHNAME}",
  "buildid": "${BUILD_BUILDID}",
  "buildnumber": "${BUILD_BUILDNUMBER}",
  "githash": "${BUILD_SOURCEVERSION}",
  "changemsg": "${BUILD_SOURCEVERSIONMESSAGE}"
}
EOF
}

if [ "$PLATFORM" = "windows-x64" ]; then
  mv $STAGE/version/build/Release/* $STAGE/version/build/
  rm -d $STAGE/version/build/Release
fi


echo "$(generate_metadata)" > $STAGE/latest_${BUILD_SOURCEBRANCHNAME}_${PLATFORM}.json
mv "$STAGE/version/build" "$STAGE/version/$PLATFORM"
mv "$STAGE/version" "$STAGE/$VERSION"
