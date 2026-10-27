# Building from source

You need your own copy of the game (USA `default.xbe`): the recompiler reads it at build time. It never goes in
this repository.

## Tools

- Windows 10 or 11, 64-bit
- Visual Studio 2022 (Community is fine) with **Desktop development with C++**, plus the **C++ Clang tools** and
  **CMake** components
- Git

## Steps

In a PowerShell window:

```powershell
# 1. the code, with the XboxXBE-Recomp SDK as a submodule
git clone --recurse-submodules https://github.com/<you>/<this-repo>.git bf2mc
cd bf2mc

# 2. apply this port's patches to the SDK, in name order
Get-ChildItem patches -Filter *.patch | Sort-Object Name | ForEach-Object {
  git -C sdk apply --whitespace=nowarn "../patches/$($_.Name)"
}

# 3. optional: NVIDIA NVAPI for Reflex (without it the build works, and Reflex shows as not available)
git clone https://github.com/NVIDIA/nvapi nvapi

# 4. your own game's XBE
New-Item -ItemType Directory -Force assets | Out-Null
Copy-Item "D:\path\to\your\game\default.xbe" assets\default.xbe
```

Then either run `build.bat`, or, from a *x64 Native Tools Command Prompt for VS 2022*:

```bat
cmake --preset release
cmake --build out/build/release --target BF2MC xbe_recomp compiler_files
```

The exe and the `compiler` folder end up in `out/build/release`. To play, copy `BF2MC.exe`, the `compiler` folder and
the Visual C++ runtime DLLs into a folder with an `assets` folder holding the game (see the README), or use
`release.bat`.

## How it fits together

| Path | What |
|---|---|
| `sdk/` | [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp), pinned to a known commit |
| `patches/` | this port's changes to the SDK (renderer, latency, HUD layout, Reflex, SSAO), applied before building |
| `config/game.toml` | the recompiler's input: the XDK functions found in the XBE, and this port's hooks |
| `src/common/` | the host side: settings, the F10 panel, mouse and keyboard, controller layout |
| `src/game/` | hooks into the game's code: `X86_FUNC(sub_XXXXXXXX)` replaces the recompiled function at that address |

The game code itself is not built here, and the release contains none of it. The exe builds `BF2MC.dll` on the
player's PC from their own XBE, using the bundled compiler.
