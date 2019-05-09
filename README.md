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

The API is in C, and bridged into C# and Erlang.

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
