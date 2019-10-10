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

## Using from different languages

Since allonet is written in C, its native API is C headers located in `include/allonet`.

### Lua

The dylib/so/dll file is loadable by Lua. To simplify loading it, you can just 
`require('allonet/lang/lua/allonet.lua')` and it will load the binary for you.

### C#

The C# bridge is in the `allovisor` repo.

### Erlang

The Erlang bridge is in the `alloplace` repo.

## Developing allonet

Allonet uses Cmake to generate its build files. To build using Make on a Unix/Mac machine,
do:

1. `mkdir build; cd build`
2. `cmake ..`
3. `make`

This will build allonet binaries. You could try out `./allodummyclient alloplace://nevyn.places.alloverse.com`
to make sure it works.

To develop using e g Xcode, you can just supply `-G` to Cmake.

1. `mkdir xcbuild; cd xcbuild`
2. `cmake -G Xcode ..`
3. `open *.xcodeproj`

The same applies to Visual Studio, etc...

Visual Studio Code has great Cmake plugins, so you can just automate the above process
and just click "build" in the UI.

## Progress

So far, extremely rudimentary. Todo in order of priority:

- [x] Switch out manual arrays for libsvc
- [x] Basic Server implementation on enet
- [x] Basic Client implementation on enet
- [x] C# bridge and integrate with Unity visor
- [x] Erlang bridge and make sure server API fits mm pattern
- [x] Move to component based entities
- [x] Flesh out intents (head and hands, pointing, clicking)
- [ ] Assets (geometry, textures, animations)
- [ ] Static geometry (i e the place's environment)
- [ ] Manipulation intents (grabbing, throwing, menuing, ...)
- [ ] ACLs
- [ ] Avatar (specify asset for entity, and then animate/manipulate it)
- [ ] Multiple entities per client
- [ ] Move from enet to webrtc data channels
- [ ] Audio channels
- [ ] Video channels
