# FFMPEG packaged for Alloverse

LGPL builds of ffmpeg 4.4.1-2 (aka `cc33e73618` aka `release/4.4`)

## Windows

We use the Windows builds from [BtbN](https://github.com/BtbN/FFmpeg-Builds/releases).
The files in `windows-x64` is a subset from those at
 https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2021-12-02-12-20/ffmpeg-n4.4.1-2-gcc33e73618-win64-lgpl-shared-4.4.zip.

## Android / Oculus Quest

Built from source based on [@donturner's medium article](https://medium.com/@donturner/using-ffmpeg-for-faster-audio-decoding-967894e94e71).

1. clone ffmpeg sources: `git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg`
2. `git checkout release/4.4` (should end up on a `HEAD` of `cc33e73618`)
3. Put the script below into `build.sh` in the root of the repo
4. Modify `HOST_OS_ARCH` to match your computer
5. Invoke appropriately for your computer, e g `env ANDROID_NDK=$HOME/Android/Sdk/ndk/22.1.7171670 bash build.sh`

The article's script is modified to only build for arm64-v8a (Quest's arch),
and to include all lgpl features (rather than just mp3):

```
#!/bin/bash

set -ex

if [ -z "$ANDROID_NDK" ]; then
  echo "Please set ANDROID_NDK to the Android NDK folder"
  exit 1
fi

#Change to your local machine's architecture
HOST_OS_ARCH=linux-x86_64

function configure_ffmpeg {

  ABI=$1
  PLATFORM_VERSION=$2
  TOOLCHAIN_PATH=$ANDROID_NDK/toolchains/llvm/prebuilt/${HOST_OS_ARCH}/bin
  local STRIP_COMMAND

  # Determine the architecture specific options to use
  case ${ABI} in
  armeabi-v7a)
    TOOLCHAIN_PREFIX=armv7a-linux-androideabi
    STRIP_COMMAND=arm-linux-androideabi-strip
    ARCH=armv7-a
    ;;
  arm64-v8a)
    TOOLCHAIN_PREFIX=aarch64-linux-android
    ARCH=aarch64
    ;;
  x86)
    TOOLCHAIN_PREFIX=i686-linux-android
    ARCH=x86
    EXTRA_CONFIG="--disable-asm"
    ;;
  x86_64)
    TOOLCHAIN_PREFIX=x86_64-linux-android
    ARCH=x86_64
    EXTRA_CONFIG="--disable-asm"
    ;;
  esac

  if [ -z ${STRIP_COMMAND} ]; then
    STRIP_COMMAND=${TOOLCHAIN_PREFIX}-strip
  fi

  echo "Configuring FFmpeg build for ${ABI}"
  echo "Toolchain path ${TOOLCHAIN_PATH}"
  echo "Command prefix ${TOOLCHAIN_PREFIX}"
  echo "Strip command ${STRIP_COMMAND}"

  ./configure \
  --prefix=build/${ABI} \
  --target-os=android \
  --arch=${ARCH} \
  --enable-cross-compile \
  --cc=${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}${PLATFORM_VERSION}-clang \
  --strip=${TOOLCHAIN_PATH}/${STRIP_COMMAND} \
  --enable-small \
  --disable-programs \
  --disable-doc \
  --enable-shared \
  --disable-static \
  ${EXTRA_CONFIG}
  
  return $?
}

function build_ffmpeg {

  configure_ffmpeg $1 $2

  if [ $? -eq 0 ]
  then
          make clean
          make -j12
  else
          echo "FFmpeg configuration failed, please check the error log."
  fi
}

build_ffmpeg arm64-v8a 26
```

## Linux

Very similar to Android, except I didn't bother with a script:

    git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg
    cd ffmpeg; git checkout release/4.4
    ./configure --enable-small --disable-programs --disable-doc --enable-shared --disable-static
    make -j12

## macOS

Similar to Linux, except we have to lipo `arm64` and `x86_64` binaries. On an Apple Silicon mac:

    git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg
    cd ffmpeg; git checkout release/4.4
    ./configure --enable-small --disable-programs --disable-doc --enable-shared --disable-static
    make -j12
    mkdir ../arm
    cp */*.dylib ../arm/
    git clean -dffx
    arch --x86_64 bash
    ./configure --enable-small --disable-programs --disable-doc --enable-shared --disable-static --arch=x86_64
    make -j12
    mkdir ../x64
    cp */*.dylib ../x64
    mkdir ../uni
    cd ../arm
    for $b in `ls` do;
        lipo -create -output ../uni/$b $b ../x64/$b
    fi
    install_name_tool -id @executable_path/libswscale.dylib libswscale.dylib
    install_name_tool -id @executable_path/libavformat.dylib libavformat.dylib
    install_name_tool -id @executable_path/libavutil.dylib libavutil.dylib
    install_name_tool -id @executable_path/libavcodec.dylib libavcodec.dylib
    install_name_tool -id @executable_path/libswresample.dylib libswresample.dylib
    for tool in *.dylib; install_name_tool -change /usr/local/lib/libavcodec.58.dylib @executable_path/libavcodec.dylib $tool; end
    for tool in *.dylib; install_name_tool -change /usr/local/lib/libswresample.3.dylib @executable_path/libswresample.dylib $tool; end
    for tool in *.dylib; install_name_tool -change /usr/local/lib/libavutil.56.dylib @executable_path/libavutil.dylib $tool; end
    
