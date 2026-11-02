# GTA-IV-HD_Rumble 1.0

> **Based on the original mod:** GTA-IV-HD_Rumble is an unofficial enhanced and extended native C++ version of **RumbleNutsClassic / Advanced Controller Rumble Vehicles-WIP**, created by **eg** and uploaded by **snelis**. [Original mod on Nexus Mods](https://www.nexusmods.com/gta4/mods/712?tab=description). Full credit for the original mod and its foundation goes to its author. This version adds configurable feedback and optional Linux impulse-trigger support; it is not an official release by the original author.

> **AI assistance:** This project was developed with the assistance of OpenAI Codex, an AI coding assistant.

Native C++ ASI rumble mod for **GTA IV Complete Edition, GTAIV.exe 1.2.0.59 (32-bit)**. No .NET or ScriptHookDotNet required.

Adds vehicle feedback for acceleration, braking, speed-drop/gear effects, bumps, turns, damage and burst tyres. Optional **Xbox Series impulse triggers, developed primarily for Linux,** use a companion bridge while **Steam Input stays enabled**. The bridge presents a virtual Xbox Series controller, including the Share button mapping.

This is an unofficial modified port of **RumbleNutsClassic / Advanced Controller Rumble Vehicles-WIP** by **eg**, uploaded by **snelis**: [original mod](https://www.nexusmods.com/gta4/mods/712). See [CREDITS.md](CREDITS.md) for attribution and usage conditions.

## Install

1. Close GTA IV. Keep your working Proton, FusionFix and Steam Input configuration.
2. Install a working 32-bit ASI Loader with `InitializeASI` support, such as the loader included with [FusionFix](https://github.com/ThirteenAG/GTAIV.EFLC.FusionFix).
3. Copy **GTA-IV-HD_Rumble.asi** and **GTA-IV-HD_Rumble.ini** into `GTAIV/scripts/`, beside the directory containing `GTAIV.exe`. Back up any previous versions. Remove the previous `RumbleNuts.asi` when upgrading to avoid loading both plugins, and disable the original RumbleNuts C# script. The configuration file is now `GTA-IV-HD_Rumble.ini`, and its main section is `[GTA-IV-HD_Rumble]`.
4. For ordinary rumble only, set `[Triggers] Enabled=0` and launch the game normally.
5. For impulse triggers, keep this whole package in a permanent folder and follow [Linux bridge setup](bridge/README.md). Only the ASI and INI belong in the game folder.

### One-click launch on Linux

Once the bridge prerequisites are satisfied:

```bash
python3 create-shortcuts.py
```

This creates **GTA IV + GTA-IV-HD_Rumble** and **Stop GTA-IV-HD_Rumble bridge** desktop shortcuts. The first starts the bridge, waits for readiness, then opens GTA IV through Steam. After closing the game, use the second to stop the bridge and restore normal physical-controller input. The bridge does **not** stop automatically when the game exits and does **not** start automatically at login.

Alternatively run `bash Play-GTA-IV.sh` and, after playing, `bash Stop-GTA-IV-HD_Rumble.sh`. Use these from the **host desktop**, not a container. Keep Steam Input enabled. Recreate shortcuts if you move the package folder.

## Steam Deck: ordinary rumble

The user confirmed ordinary rumble works in Gaming Mode with Steam Input enabled. Install the ASI/INI, set `[Triggers] Enabled=0`, and launch GTA IV normally. No Linux bridge, shell script or desktop shortcut is required for ordinary rumble. The impulse-trigger bridge is only for a supported external USB Xbox Series controller, not the Deck built-in triggers.

## Controls

| Key | Action |
| --- | --- |
| P | Cycle Medium → Light → Strong; selection lasts until game exit |
| O | Toggle diagnostic overlay: profile, vehicle group, speed, current/last motor command |
| F10 | Display vehicle model hash; optionally save it beside the ASI |

## Configuration

Edit **the INI next to the installed ASI**, then restart GTA IV. The defaults preserve the previously tuned weak rumble:

- Equal strength across profiles; different activation thresholds/cooldowns.
- Main motor ceiling **1500/65535**, continuous tyre strength **300** before gains.
- Stock shooting commands **×1.5** on foot and in vehicles.
- RT for acceleration and on-foot shooting, LT for braking. **No trigger recoil for in-car shooting** by default.

**[CONFIG.md](CONFIG.md)** documents every setting, range, interaction and threshold.

## Compatibility and limitations

- The earlier 0.5.2 effects were confirmed by the user on **Bazzite/Proton, USB Xbox Series 045e:0b12, Steam Input enabled**. The user also confirmed ordinary rumble on Steam Deck in Gaming Mode. Version 1.0 is the renamed release; its rebuilt binaries have not yet been retested in-game. See [VALIDATION.md](VALIDATION.md).
- Bluetooth, wireless dongles, other external controller models and other GTA IV versions are unverified. The Linux bridge intentionally targets one USB `045e:0b12` device. Ordinary ASI/XInput rumble does not require that bridge.
- Share is exposed in the virtual Series mapping; its Steam action and the new virtual identity still need user confirmation. A virtual device can use different Steam bindings from the physical one.
- Effects estimate motion from speed/position, not actual RPM, gears, road surface or brake-pedal input. Coasting can trigger braking feedback; stock vehicle grouping inaccuracies remain.
- Shooting is detected in a short time window. The first shot may precede detection, and unrelated stock effects in that window can also be scaled. The game must resolve `XInputSetState` through the hooked path; the log reports when this happens.
- During shooting, vehicle effects yield for 650 ms after detection. This is not a full mixer for all other mods. Trigger recoil derives from stock shooting rumble; no stock command means no synthetic trigger recoil.
- On pause/focus loss, commands are stopped or expire. The bridge reconnects `xpad` on normal shutdown; reconnect USB after an abnormal termination if necessary.
- Do not replace/unload the ASI while GTA IV is running. Controller strength is hardware-dependent.

## Build and test

ASI: 32-bit MinGW-w64 C++17 (`i686-w64-mingw32-g++`):

```bash
bash build.sh
```

Linux bridge: C++17 compiler, `pkg-config`, SDL3 development headers and a runtime with direct GIP support:

```bash
bash build-bridge.sh
```

The included bridge is Linux x86_64, built against the tested Bazzite SDL3 3.4.18 runtime. Other distributions may need a rebuild; private SDL libraries are not bundled.

Engine/config tests (ordinary Linux C++17 compiler):

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Werror tests/engine_test.cpp -o engine_test
./engine_test
python3 tests/launcher_test.py
```

Optional native-hash audit: `python3 tests/check_api.py /path/to/FusionFix/source/natives.ixx`.

`SHA256SUMS.txt` covers the shipped ASI and bridge. Rebuilds require updated checksums. Logs, personal backups and game files are excluded from this distribution.
