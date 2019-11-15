Creating a project with the swift binding
---

Until someone understands CMake enough to have it creating a framework project with the internal CAllonet/module.modulemap and ios/tvos/mac etc, here's the manual way:

1. Edit the allonet CMakeList.txt
  - Turn ON static library
  - Turn OFF LUA
1. `mkdir build & cd build & cmake -GXcode ..`
1. Drag in `build/allonet.xcodeproj` to you project
1. Link your project with `liballonet.a`, `libcjson.a`, `libenet.a`
1. Add the *lang/swift/Allonet* folder to your target
1. Find your targets **Import Paths** (`SWIFT_INCLUDE_PATHS`) Build Setting
  - Add the path to the *lang/swift/Allonet/CAllonet/* folder
1. Find your targets **Header Search Paths** (`HEADER_SEARCH_PATHS`) Build Setting
  - Add the path to *allonet/include*
