# Changelog

All notable changes to this project are documented here. Dev builds are
published as a rolling `dev` pre-release and track the Unreleased section
below; a dated entry is added when a versioned release is cut.

## [Unreleased]

### Changed
- Settings move to `media\CameraUnlock.ini`. Earlier versions of the mod kept these settings in `Yakuza0HeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `Yakuza0HeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `Yakuza0HeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default earlier versions used, because `Yakuza0HeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor is this, where your old file had it:
  - A hotkey set to Ctrl, Shift or Alt on its own. That key goes down before the key of any chord made with it, so the hotkey is left unbound, and it keeps its Ctrl+Shift chord where it has one.
- An older version of the mod reads `Yakuza0HeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `Yakuza0HeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `Yakuza0HeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. `YawModeKey` is no longer a virtual-key code, and End, Page Up and the chords, which earlier versions bound in code, can now be rebound or removed.
- The tracking mode and the yaw mode are saved to `CameraUnlock.ini` the moment a hotkey changes them, and come back at the next launch. Earlier versions started in rotation and position with the yaw mode from the config file every time. End changes the current session only, as before.
- The uninstaller leaves `media\CameraUnlock.ini` and `media\Yakuza0HeadTracking.ini` in place. Earlier versions' uninstaller deleted `Yakuza0HeadTracking.ini`.

### Added
- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.
- `CameraUnlock.ini` has settings for the UDP port (`UdpPort`), whether tracking is on at startup (`EnableOnStartup`), the tracking mode at startup (`RotationEnabled` and `PositionEnabled`), smoothing (`LocalSmoothing`, `RemoteSmoothing`), how far each lean can move the view (`PositionLimitX`, `PositionLimitY`, `PositionLimitYDown`, `PositionLimitZ`, `PositionLimitZBack`), and a key list for each hotkey (`ToggleKey`, `CycleTrackingModeKey`, `YawModeKey`). Their built-in values are the ones earlier versions used.
- Single previous log generation: the launch before the current one is kept as
  `Yakuza0HeadTracking.prev.log`, so a crash the user only fetches the log for
  after relaunching is still diagnosable.
- Initial repo scaffold from cameraunlock-core templates (C++ ASI mod).
- Ultimate ASI Loader install/uninstall scripts.
- CMake project producing `Yakuza0HeadTracking.asi`.
- Yakuza 0 added to `cameraunlock-core/data/games.json`.
- Camera hook via runtime pattern scan (RVA logged on match, not pinned):
  5-byte detour into a near-page thunk and naked MASM trampoline
  (`camera_hook.asm`). Snapshots clean xmm4/5/6 + FOV into a `CameraState`
  buffer each call, applies the head-tracked camera vectors, writes back
  focus and up.
- Hook-install hardening: threads suspended (with retry) before the 5-byte
  rewrite, W^X near-thunk page, and a pattern-ambiguity failsafe that
  refuses to hook (stays dormant) if the signature matches more than once.
- Gameplay-camera gating: each fire is classified by the camera object's
  vtable RVA against an allow-list (`IsGameplayCamera`), suppressing
  tracking in cutscenes and menus.
- 6DOF tracking. Rotation rewrites the camera focus/up; position applies a
  translation in the clean (pre-rotation) camera basis so the offset follows
  body orientation. Pitch inverted, X/Z inverted, asymmetric Z clamp
  (0.10 m back from the engine's perspective, 0.40 m forward).
- World-space (horizon-locked) and camera-local yaw modes, switchable at
  runtime and persisted via INI.
- Runtime tracking-mode cycling: rotation + position / rotation only /
  position only.
- OpenTrack UDP receiver on port 4242 via cameraunlock-core's `UdpReceiver`,
  driving `HeadTrackingSession` (processor + position processor). The socket
  binds all interfaces (`INADDR_ANY`), so a phone or other device on the LAN
  can send to it directly.
- Hotkeys: End (toggle), Page Up (cycle tracking mode), Page Down (toggle yaw
  mode), plus Ctrl+Shift+Y / G / H chord equivalents. Polled at ~60 Hz on a
  background thread.
- File logger at `Yakuza0HeadTracking.log` next to the .asi.
- Camera telemetry build switch for per-fire frame-state logging.

### Changed
- The mod keeps no centre of its own and applies the tracker pose as sent. There
  is no recenter hotkey; centre in your tracker app instead (OpenTrack's Center
  bind, or the CENTER button in Headcam). A mod-side centre sat in series with
  the tracker's own and the two drifted apart.
- The periodic camera state dump moved from every second to every five, and
  the raw view matrix line is now written only for the first ten dumps. At the
  old cadence the three lines put roughly 1.7 MB an hour into the log a user is
  asked to send, burying the startup chain.
- Tracking smoothing no longer has an enforced floor. Every connection used
  to get the doctrine baseline of 0.15; smoothing is now picked per
  connection from the packet source address, 0.0 for a tracker on this
  machine and 0.15 for a remote device. Users on a local tracker will feel
  this: input is lower latency but also less damped. `LocalSmoothing=0.15` in
  `CameraUnlock.ini` brings the old floor back. That per-connection choice is only meaningful because the receiver
  binds all interfaces, which it has always done; an earlier entry here
  described it as loopback-only, which was never true.
