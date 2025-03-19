# allonet

[![Build Status](https://dev.azure.com/alloverse/allonet/_apis/build/status/alloverse.allonet?branchName=master)](https://dev.azure.com/alloverse/allonet/_build/latest?definitionId=1?branchName=master)

Alloverse's network API. It can be used as:

* **a 3D UI API**, when by appliances to present its UI to a place's
  inhabitants, and get callbacks when users interact with it;
* **a window manager API/game engine client API**, when used by visors
  to read the environment and inhabitants of a place in order to visualize
  it on a user's screen/headset;
* **a game engine server API**, when used by placeserv to manage clients and
  the place's environment and inhabitants.

This first implementation of allonet is if prototype quality: it uses JSON as its main
payload format, and is thus quite resource intensive. It is however rather feature complete,
implementing both world syncing, client-side interpolation, RPC, audio and video streaming,
asset distribution, clock syncing, and more.

This project will soon be superceded by its successor, [allonet2](https://github.com/alloverse/allonet2),
which aims to implement all of these features in a more efficient, secure and future-proof way.

## Using from different languages

Since allonet is written in C, its native API is C headers located in `include/allonet`.

For information about specific language bridges, see the README files in the `lang`
folder (both source and binary distributions).

For C#, the bridge is in the old `allovisor` repo for now. The Erlang bridge is
in the `alloplace` repo. Both will be moved to this repo.

## Download allonet

If you don't want to build allonet from source (as per "Developing allonet" below),
you can [download a binary distribution from Azure 
Pipelines](https://dev.azure.com/alloverse/allonet/_build?definitionId=1). Select
the latest pinned build, click the "artifacts" button, and download for the platform
you want to use. All language bindings are available in each platform download.

## Developing allonet

Allonet uses Cmake to generate its build files. To build using Make on a Unix/Mac machine,
do:

1. `mkdir build; cd build`
2. `cmake ..`
3. `make`

This will build allonet binaries. You could try out `build/allodummyclient alloplace://nevyn.places.alloverse.com`
to make sure it works.

To develop using e g Xcode, you can just supply `-G` to Cmake.

1. `mkdir xcbuild; cd xcbuild`
2. `cmake -G Xcode ..`
3. `open *.xcodeproj`

The same applies to Visual Studio, etc...

Visual Studio Code has great Cmake plugins, so you can just automate the above process
and just click "build" in the UI.

### Developing for Android

1. Install Android Studio, ?? SDK, ?? NDK.
2. `mkdir build; cd build`
3. `cmake -DCMAKE_TOOLCHAIN_FILE=~/Library/Android/sdk/ndk-bundle/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a ..`.
   Path to android toolchain file may be different on your system (e g if you're running Linux or Windows).
4. `make allonet`

### Versioning

The allonet library has a major.minor.micro versioning scheme where:

* major is also the protocol version. If the wire protocol becomes incompatible,
  bump this version to have client/server auto-reject non-compatible versions.
* minor is bumped whenever there's a big new nice feature
* micro is distance-to-tag, so the number of commits since the latest 'minor' tag.

Create new tags with 

```
git tag -am "version description" x.y HEAD
git push --tags
```

### Notes for specific languages

Lua: `lib/lua` must be point at 5.1 to be luajit compatible. (Alloverse Visor uses LÖVR which uses luajit).
