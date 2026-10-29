# RetroJapanOverlay

Windows C++20/Direct3D 11 desktop overlay with procedural ASCII/particle scenes and tray controls.
The current presentation path uses GPU rendering, CPU readback and **UpdateLayeredWindow**.
The included DirectCompositionHost source is not the active presentation path.

## Build

Install Visual Studio 2022 C++ Build Tools, a Windows SDK and CMake 3.21+:

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The build copies HLSL source next to the executable; it uses fxc when found and the source fallback
otherwise. Generated build/executable files are not part of this source release.

The global **O → L → K** key-state sequence toggles visibility; tray controls remain available.
Settings/logs are local under LocalAppData. Startup is opt-in. Read the source before enabling
global desktop behavior; no overlay process or startup/registry change was invoked for this audit.


See [VALIDATION.md](VALIDATION.md) for actual publication checks.
