# Credits

## XboxXBE-Recomp, by TGP482

**[XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp)** by **[TGP482](https://github.com/TGP482)** is what
makes this port possible. It provides:
- the static recompiler that turns the original Xbox x86 code into C++;
- the Xbox kernel;
- Direct3D 8 on Direct3D 12;
- DirectSound on XAudio2;
- the input, settings, logging and packaging runtime.

This project is a game-specific layer on top of it: hooks, settings and a handful of renderer patches. The approach
in TGP482's own Xbox recompilation work (BLACK-XBERecomp) was also a reference for performance work here.

**All credit for the recompilation technology belongs to TGP482.**

XboxXBE-Recomp: MIT License, Copyright (c) 2026 TGP482.

## The original game

*Battlefield 2: Modern Combat* was developed by **Digital Illusions CE (DICE)** and published by
**Electronic Arts** in 2005. *Battlefield* is a trademark of Electronic Arts Inc. This project is not affiliated with
or endorsed by EA or DICE, and contains none of the game's code or content. Please support the original creators.

## This port

**TollieMAN88**: the port and its features (mouse and keyboard, the settings panel, ultrawide and HUD layout,
latency work, SSAO), developed with the help of **Claude** (Anthropic).

## Third-party software

These ship inside, or alongside, the release. Each keeps its own licence. The full texts are in `THIRD_PARTY.txt`
and the other licence files in every release zip.

| Component | Use | Licence |
|---|---|---|
| [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) | recompiler and runtime | MIT |
| [SDL3](https://github.com/libsdl-org/SDL) | controllers | zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | the F10 settings panel | MIT |
| [toml++](https://github.com/marzer/tomlplusplus) | `settings.toml` | MIT |
| [Zydis](https://github.com/zyantific/zydis) | x86 decoding in the recompiler | MIT |
| [LLVM / Clang / LLD](https://llvm.org) | bundled compiler that builds the game code on your PC | Apache 2.0 with LLVM exceptions |
| [NVIDIA NVAPI](https://github.com/NVIDIA/nvapi) | NVIDIA Reflex | MIT (`NVAPI_LICENSE.txt`) |
| Microsoft Visual C++ runtime | `msvcp140.dll`, `vcruntime140*.dll` | Microsoft Visual C++ redistributable licence |

NVIDIA and Reflex are trademarks of NVIDIA Corporation. Windows, Xbox and DirectX are trademarks of Microsoft
Corporation.
