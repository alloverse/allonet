# Allonet C# bridge

This library allows you to build an Alloverse app or visor using the C# language, on top of dotnet core. It is currently a work in progress, and we recommend using Lua to build apps for now.

To run the sample application, make sure the allonet library path is in your linker's loader path. E g on linux, `mkdir build; cd build; cmake ..; sudo make install` in the repo root; or put allonet.dll in this folder on windows.

You might have to change `Allonet/_Interfaces.cs`'s `   Allonet._AlloClient._dllLocation` to something that fits your system better.

To run the sample app, do:
```
    env COMPlus_DebugWriteToStdErr=1 dotnet run -c Debug --project AppSample/AppSample.csproj alloplace://nevyn.places.alloverse.com
```

Good luck!
