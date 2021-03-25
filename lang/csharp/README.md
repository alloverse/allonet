# Allonet C# bridge

This library allows you to build an Alloverse app or visor using the C# language, on top of dotnet core. It is mostly a low-level stub, and it's recommended that you use https://github.com/alloverse/alloui-csharp since it has a lot of higher-level abstractions that will help you. That library is also available on NuGet, so it's a lot easier to get started.

To run the sample application, make sure the allonet library path is in your linker's loader path. E g on linux, `mkdir build; cd build; cmake ..; sudo make install` in the repo root; or put `allonet.dll` in this folder on windows.

You might have to change `Allonet/_Interfaces.cs`'s `Allonet._AlloClient._dllLocation` to something that fits your system better.

To run the sample app, do:
```
    env COMPlus_DebugWriteToStdErr=1 dotnet run -c Debug --project ClientSample/ClientSample.csproj alloplace://nevyn.places.alloverse.com
```

Good luck!
