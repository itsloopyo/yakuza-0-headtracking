> [!CAUTION]
> ## Experimental prototype - expect missing core features
>
> This is **not** a finished mod.
>
> Current builds may only test whether head tracking can drive the camera. Bug fixes and core features like decoupled look/aim, independent reticle behavior, correct shot direction, off-screen reticle support, movement handling, and comfort tuning may be missing at this early stage of development.

# Yakuza 0 Head Tracking

Head tracking for Yakuza 0 on PC: move your head to look around while the mouse/controller still controls aim and movement, driven by [OpenTrack](https://github.com/opentrack/opentrack) or any compatible UDP head-pose source, no VR headset required.

<!-- ![Mod GIF](https://raw.githubusercontent.com/itsloopyo/yakuza-0-headtracking/main/assets/readme-clip.gif) -->

## Features

- **Decoupled look and aim** - your head moves the camera; the mouse/controller still controls aim and movement.
- **Works with any OpenTrack-compatible tracker** - phone apps, webcam trackers, VR headsets, TrackIR.
- **World-space or camera-local yaw** - horizon-locked yaw by default, toggleable in-game.

## Requirements

- [Yakuza 0 on Steam](https://store.steampowered.com/app/638970/Yakuza_0/), latest patch.
- A head tracking source: [OpenTrack](https://github.com/opentrack/opentrack) with a webcam, VR headset, TrackIR, or a phone tracking app.
- Windows 10/11, 64-bit.

## Installation

1. Download the latest `Yakuza0HeadTracking-vX.Y.Z-installer.zip` from [Releases](https://github.com/itsloopyo/yakuza-0-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`. It auto-detects your Steam install, places the mod next to `Yakuza0.exe`, and sets up Ultimate ASI Loader if it isn't already present.
4. Configure OpenTrack (or your phone app) to output UDP to `127.0.0.1:4242` (see [Setting Up OpenTrack](#setting-up-opentrack)).
5. Launch the game.

If the installer can't find your game, point it at the install folder yourself, either way works:

```powershell
# Option 1: environment variable
$env:YAKUZA_0_PATH = "D:\Games\Yakuza 0"; .\install.cmd

# Option 2: pass the path directly
.\install.cmd "D:\Games\Yakuza 0"
```

### Manual Installation

For users who prefer to place files by hand:

1. Download [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) (Win64 `dinput8.zip`), extract `dinput8.dll`, rename it to `winmm.dll`, and place it in `media\` next to `Yakuza0.exe`.
2. Place `Yakuza0HeadTracking.asi` (from the installer ZIP's `plugins\` folder) in the same `media\` folder.

Alternatively, the Nexus ZIP (`Yakuza0HeadTracking-vX.Y.Z-nexus.zip`) extracts directly into the game's install folder; it contains only `media\Yakuza0HeadTracking.asi`, so you still need an ASI loader installed.

## Setting Up OpenTrack

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Set **Output** to `UDP over network`, IP `127.0.0.1`, port `4242`.
3. Set **Input** to whichever tracker source you use (see below).
4. Click **Start** before or after launching the game.

### VR Headset Setup

1. Connect your headset to the PC (Quest: Air Link or Virtual Desktop).
2. Start SteamVR.
3. In OpenTrack, set **Input** to `SteamVR`.

### Webcam Setup

1. In OpenTrack, set **Input** to `neuralnet tracker`.
2. Select your webcam in the input settings.

### Phone App Setup

1. Install a head tracking app that sends the OpenTrack UDP protocol (such as SmoothTrack).
2. If the app does its own smoothing, point it directly at your PC's IP, port `4242`.
3. For curve mapping or extra filtering, send the app's output to OpenTrack first (input `UDP over network`) and let OpenTrack relay to `127.0.0.1:4242`.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord           |
|---------------------|-------------|-----------------|
| Recenter            | `Home`      | `Ctrl+Shift+T`  |
| Toggle tracking     | `End`       | `Ctrl+Shift+Y`  |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G`  |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H`  |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

`Page Down` / `Ctrl+Shift+H` toggles between world-space (horizon-locked) yaw and camera-local yaw. The toggle takes effect immediately and resets to the configured default on the next launch.

## Configuration

`Yakuza0HeadTracking.ini` is created next to `Yakuza0HeadTracking.asi` (in `media\`, beside `Yakuza0.exe`) on first launch. Missing entries fall back to their defaults.

```ini
[General]
; Yaw mode: true = horizon-locked yaw (default), false = camera-local
WorldSpaceYaw=true

[Hotkeys]
; Page Down - toggle world/local yaw
YawModeKey=0x22
```

- `WorldSpaceYaw=true` keeps yaw rotating around the world up-axis regardless of camera pitch, so turning your head while looking at the floor still pans across the floor. Set to `false` for camera-local yaw, which follows the camera's current up-axis (leans at extreme pitch).
- `YawModeKey` is the Windows virtual-key code for the yaw mode toggle (default `0x22`, Page Down).

## Troubleshooting

**Mod not loading:**

- Check for `Yakuza0HeadTracking.log` in `media\` next to `Yakuza0.exe` after launching the game. If it's missing, the ASI loader isn't engaging; re-run `install.cmd`.
- Verify both `winmm.dll` and `Yakuza0HeadTracking.asi` are in `media\`.

**No tracking response:**

- Confirm OpenTrack's output is `UDP over network` to `127.0.0.1`, port `4242`, and that OpenTrack is started.
- Make sure only one source is sending to port 4242 (don't run a phone app and OpenTrack at the same time pointing at the same port).
- Tracking may have been toggled off; press `End` (or `Ctrl+Shift+Y`).
- The game pauses when its window loses focus, so keep it focused while testing.

**Jittery / unstable tracking:**

- Enable a smoothing filter in OpenTrack (Accela is the default and works well).
- Wireless phone trackers on congested WiFi can stutter; move closer to the router or switch to a webcam tracker.

**Yaw feels wrong when looking up or down at extreme angles:**

- Try toggling between world-locked and camera-local yaw with `Page Down`. World-locked (default) is horizon-stable; camera-local follows the camera's current up-axis.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd` from the installer ZIP. This removes the mod files. The ASI loader is only removed if the installer put it there; use `uninstall.cmd /force` to remove it anyway.

## Building from Source

Requires Visual Studio 2022 Build Tools, CMake 3.20+, and [pixi](https://pixi.sh).

```powershell
git clone --recurse-submodules https://github.com/itsloopyo/yakuza-0-headtracking
cd yakuza-0-headtracking
pixi run build
pixi run test
pixi run package
```

## License

MIT License - see [LICENSE](LICENSE) for details. Third-party components are listed in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Credits

- Ryu Ga Gotoku Studio / Sega for Yakuza 0.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (MIT).
- [OpenTrack](https://github.com/opentrack/opentrack) (ISC).
- [etra0/yakuza-freecam](https://github.com/etra0/yakuza-freecam) (MIT) for the camera-hook reverse-engineering reference.
- [CameraUnlock core](https://github.com/itsloopyo/cameraunlock-core) shared library.

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Ryu Ga Gotoku Studio or Sega. Use at your own risk.
