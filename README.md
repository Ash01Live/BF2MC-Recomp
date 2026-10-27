# BF2MC Recomp

**An unofficial, fan-made PC port of the original Xbox version of *Battlefield 2: Modern Combat* (USA).**

The game's original Xbox code is statically recompiled into a native Windows program. It is built entirely on
**[XboxXBE-Recomp by TGP482](https://github.com/TGP482/XboxXBE-Recomp)**. That SDK does all the heavy lifting:
recompiling the code, running the Xbox kernel, Direct3D, audio and input on a PC. Everything here is a game-specific
layer on top of it. See [CREDITS.md](CREDITS.md).

> **This is a rough beta and an offline-only, single-player port.** Expect bugs. Read
> [Known issues](#known-issues) before playing.

## A note from the author

This is my **first ever project**. I'm a total beginner: I'd never done anything like this before, and I built it
by learning as I went, with an AI assistant (Claude) doing much of the heavy technical lifting. So please go easy
on me. There will be rough code, odd decisions and things a more experienced person would do differently.

**Please treat this as a framework, not a finished product.** I'd love for people who know more than me to take it
and run with it:
- fork it;
- fix what I got wrong;
- improve the renderer, the HUD and the controls;
- add what's missing (a mouse pointer in the menus, proper HUD art, support for other versions of the game, and
  much more).

Pull requests, forks and your own spin-offs are all welcome, and the MIT licence lets you do whatever you like with
my part of it. If you make something better, I'd genuinely love to see it.

— TollieMAN88

---

## Important: read this first

- **No game files are included, and none will ever be provided.** You need your own copy of *Battlefield 2: Modern
  Combat* for the original Xbox (USA version), dumped from a disc you own. Please don't ask for game files, and
  don't share them in issues or discussions.
- **Offline only.** Online play, multiplayer and any connection to EA's or anyone else's servers are not supported
  and not planned. The campaign and challenge modes are what this port is for.
- **Unofficial.** This project is not affiliated with, endorsed by or connected to Electronic Arts, DICE, Microsoft
  or NVIDIA. *Battlefield* and *Battlefield 2: Modern Combat* are trademarks of Electronic Arts Inc. All other
  trademarks belong to their owners.
- **No warranty.** It's provided as-is under the [MIT licence](LICENSE). Use it at your own risk.
- **Not for sale.** This port is free. If anyone charges you for it, or bundles it with game files, that isn't
  this project.

---

## Requirements

| | Minimum |
|---|---|
| OS | Windows 10 or 11, 64-bit |
| GPU | Any graphics card with DirectX 12 support (feature level 11_0) |
| CPU | 64-bit with SSE4.2 (most CPUs from about 2011 onwards) |
| Game | *Battlefield 2: Modern Combat*, original Xbox, **USA** version |
| Disk | About 1 GB beside the game files |

Only the USA release is supported. A correct `default.xbe` has the SHA-1 hash
`7479febc78386ec9e5d8649ab5cef4a826e4fbd9`. The log shows it on every start, so you can check your dump.

**Tested on:** Windows 11, NVIDIA RTX 4080, AMD Ryzen 7 7800X3D, a 3440×1440 165 Hz ultrawide monitor, with
mouse and keyboard and an Xbox controller. **Everything else is untested**, including AMD and Intel graphics,
laptops, 16:9, 16:10 and 4K screens, Steam Deck and Linux. Reports from other setups are very welcome.

---

## Installing

1. Download the latest zip from **[Releases](../../releases)** and unzip it into a new, empty folder.
2. Put your game in the `assets` folder, either as:
   - the disc image (`.iso`), **or**
   - `default.xbe` together with all the `.viv` files and folders that sit beside it on the disc.
3. Run `BF2MC.exe`.

**The first launch takes a while (roughly 20–60 seconds).** The exe builds the game code from *your* copy of the
game, using the compiler in the `compiler` folder. This happens once per release. Later launches are quick.

**Updating:** unzip the new release over the old folder and overwrite everything, not just `BF2MC.exe`. The
`compiler` folder must match the exe. Your settings (`settings.toml`) and saves (`hdd/`) are kept.

### Antivirus warnings

The exe is not code-signed. On first launch it runs a bundled compiler (clang/LLVM) to build a DLL, then loads it.
Some antivirus tools find that pattern suspicious. If yours blocks the game, you can add the folder as an exception.
Only do that if you downloaded the zip from this repository's Releases page. The source code is all here if you'd
rather check it, or build it yourself (see [BUILDING.md](BUILDING.md)).

### Where things are kept

All of these are beside the exe:

| File / folder | What |
|---|---|
| `settings.toml` | every setting. It's rewritten on each start, so edit it only with the game closed, or use the F10 panel |
| `hdd/` | your saves |
| `logs/` | log files: attach the newest one to any bug report |
| `CrashDumps/` | crash dumps, if the game crashes |

---

## Controls

### Settings panel: **F10**

Press **F10** in game, or **Back + Start** on a controller, to open the settings panel. Every change applies straight
away and is saved. Only **Low latency** needs a restart. Esc or F10 closes it.

### Mouse and keyboard

Defaults. You can rebind every one of them in **F10 → Keyboard and mouse**: click an action, then press a key, mouse
button or the wheel.

| Action | Default |
|---|---|
| Move | W A S D |
| Look | Mouse (raw input, so your mouse driver's settings, e.g. RawAccel, carry through) |
| Fire | Left mouse button |
| Aim down sights | Right mouse button |
| Jump | Space |
| Crouch / prone | C |
| Reload | R |
| Weapon select | Hold **Alt** and flick the mouse towards the slot you want, then let go |
| Hotswap | F |
| Enter / leave vehicle | E |
| Parachute | V |
| D-pad up / right / down / left | 1 / 2 / 3 / 4 |
| Pause | Esc |
| Map / scores | Tab |
| Menus | Arrow keys to move, Enter to select, Esc or Backspace to go back |

There's no mouse pointer in the menus. Use the keyboard or a controller there.

### Controller

Any controller Windows sees as an Xbox-style pad works. With **Call of Duty layout** on (the default):

| Button | Action |
|---|---|
| Left trigger (hold) | Aim down sights |
| Right trigger | Fire |
| A | Jump |
| B | Crouch / prone |
| X | Reload |
| Y | Next weapon (hold, then the right stick picks one) |
| LB | Enter / leave vehicle |
| RB | Parachute |
| Right stick click | Hotswap |

Turn **Call of Duty layout** off in F10 to get the game's original layout back. Menus always use the game's own
buttons.

---

## Settings (F10)

| Setting | What it does |
|---|---|
| **Display mode** | Fullscreen, borderless or windowed |
| **Resolution** | Window or screen resolution. *Desktop* uses your desktop's |
| **Render resolution** | Internal resolution, 25–200%. Above 100% supersamples for a sharper image |
| **Ultrawide** | On a screen wider than 16:9, fills the whole width with more of the world (see [Screens](#screens-and-aspect-ratios)) |
| **Max frame rate** | 30 to 240, your refresh rate, or unlimited |
| **Vertical sync** | Standard vsync |
| **Low latency** *(restart)* | Holds at most one frame in the display queue, for less input lag |
| **NVIDIA Reflex** | Off, On, or On + Boost. NVIDIA GPUs only. Shows *Not available* on others |
| **FPS counter** | A small frame rate counter in the corner |
| **Anisotropic filtering** | Sharper textures at angles: game default, 2× to 16× |
| **Ambient occlusion (SSAO)** | Soft contact shadows in corners and under objects: off, on or strong *(new, see known issues)* |
| **Ambient occlusion reach** | How far that shading spreads, 0.2–5 m |
| **Motion blur and haze (game's)** | *Experimental*: on is the game as it is |
| **Field of view** | 50–200% of the game's view. Higher shows more |
| **Gun field of view** | 50–300%. Higher makes the first-person gun smaller and further away |
| **Crosshair and mission text size** | Draws the crosshair and mission text at a crisp native pixel size instead of the game's blown-up one |
| **HUD size** | Shrinks the in-game HUD (map, health, ammo, timer, name), 25–100% |
| **HUD out to the screen edges** | Moves those HUD pieces out towards the screen's edges, 0–100% |
| **Call of Duty layout** | Controller layout, see above |
| **Hipfire sensitivity** | Mouse sensitivity when not aiming. Fine steps at low values |
| **ADS sensitivity** | Mouse sensitivity while aiming down sights, separate from hipfire |
| **Invert mouse Y** | |
| **Weapon recoil** | On: the game's camera kick while firing comes through. Off: dead-steady aim |
| **Keyboard and mouse** | Rebind every action. *Reset keys to defaults* restores them |
| **Skip intros** | Straight to the menu, with no logo screens or intro movie |
| **Texture replacement / dumping** | For texture packs and texture modding |

---

## Screens and aspect ratios

Only 3440×1440 (21:9) has been tested properly. This is what to expect elsewhere:

| Screen | What to expect |
|---|---|
| **21:9 / 32:9 ultrawide** | With *Ultrawide* on, the 3D world fills the full width. Menus stay a centred 4:3 block with black sides, as the game was designed. The in-game HUD is shrunk and moved to the edges by *HUD size* and *HUD out to the screen edges* |
| **16:9** (1080p, 1440p, 4K) | Should be the simplest case: the game's own widescreen mode. HUD size works. Edge placement has less room to move |
| **16:10, 4:3** | Should work, with bars where the shape doesn't match. Untested |
| **4K and above** | Should work. Raise *Render resolution* only if your GPU has room |
| **High refresh rate** (120 Hz+) | Runs uncapped, or capped by *Max frame rate*. If anything in the game behaves oddly at high frame rates, cap it to 60 and please report it |
| **Laptops / hybrid graphics** | The port asks Windows for the high-performance GPU. Untested |

---

## Known issues

This is a beta. These are known, and most of them are rough edges rather than bugs that stop you playing:

**General**
- Offline single player only (campaign and challenges). Online and multiplayer don't work and aren't planned.
- The first launch of each release takes 20–60 seconds while it builds the game code.
- The pause before the HQ mission briefing can't be skipped.
- Only the USA version of the game is supported.

**Display and HUD**
- The HUD artwork itself is still the game's 4:3 art: it's moved and resized, not redrawn.
- An ally marker that appears and stays perfectly still for half a second may briefly jump to the screen edge before
  snapping back.
- HUD placement was tuned on 21:9 only.
- **SSAO is new:** it may show artefacts, flicker or odd dark patches in places. Turn it off in F10 if it bothers you.
- **Motion blur and haze (game's)** is experimental. Leave it on unless you're testing it.

**Controls**
- There's no mouse pointer in the menus: use the keyboard or a controller there.
- Mouse ADS detection (which sensitivity applies) is approximate and can occasionally pick the wrong one.
- With the mouse, the game's scope sway is removed, so aim stays where you put it.
- Hipfire has no camera recoil. That's the original game's behaviour. The first-person gun has no recoil animation,
  which also matches the original.
- The game has no sprint.

**Performance**
- NVIDIA Reflex works on NVIDIA GPUs only.
- *Low latency* only changes after a restart.

If you hit something not listed here, please [report it](#reporting-a-bug).

---

## Reporting a bug

Open an [issue](../../issues) using the bug report template, and include:

1. The newest log file from `logs/`. It lists your GPU, CPU, OS, screen and settings, but no personal information
   apart from the folder path, which you can blank out.
2. What happened, and what you expected.
3. Where it happened: mission, menu, and what you were doing.
4. A screenshot if it's visual.

Please **never** attach game files, and don't ask for them.

---

## Building from source

See [BUILDING.md](BUILDING.md). You'll need Windows, Visual Studio 2022 with the C++ and Clang tools, and your own
copy of the game.

---

## Contributing

Anything goes: bug fixes, new features, cleaner code, better docs, testing on hardware I don't have. Open an issue
to talk about an idea, or just send a pull request. If you'd rather take the whole thing in your own direction,
fork it, that's what it's here for. Please keep game files out of everything.

Good places to start:
- a mouse pointer for the menus;
- redrawing or properly re-laying out the HUD for widescreen;
- making ADS detection exact by finding the game's own aim state;
- testing and fixing on AMD and Intel GPUs, 16:9 and 4K screens, and Steam Deck / Linux;
- the game's PAL and other regional releases.

## Licence and credits

The code in this repository is released under the [MIT licence](LICENSE): use it, fork it, build on it.
Third-party components keep their own licences, listed in [CREDITS.md](CREDITS.md) and shipped with every release.

**All credit for the recompilation technology goes to [TGP482](https://github.com/TGP482) and
[XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp).** This project is one game-specific layer on top of it.

Port by TollieMAN88, developed with the help of Claude (Anthropic).
