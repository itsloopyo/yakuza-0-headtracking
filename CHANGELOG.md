# Changelog

All notable changes to this project are documented here. Dev builds are
published as a rolling `dev` pre-release and track the Unreleased section
below; a dated entry is added when a versioned release is cut.

## [Unreleased]

### Added
- Single previous log generation: the launch before the current one is kept as
  `Yakuza0HeadTracking.prev.log`, so a crash the user only fetches the log for
  after relaunching is still diagnosable.
- Initial repo scaffold from cameraunlock-core templates (C++ ASI mod).
- Ultimate ASI Loader install/uninstall scripts.
- CMake project producing `Yakuza0HeadTracking.asi`.
- Reverse-engineering notes for the camera hook, kept locally and not
  distributed. The hook site RVA and the register meaning at it are findings
  published by etra0/yakuza-freecam (MIT); no code was copied from it, and the
  byte signature we scan for was read off our own copy of the current Steam
  build. See THIRD-PARTY-NOTICES.md for the full breakdown.
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
- INI config (`Yakuza0HeadTracking.ini`, written next to the .asi) for
  `WorldSpaceYaw` and `YawModeKey`, with virtual-key-range validation.
- Hotkeys: End (toggle), Page Up (cycle tracking mode), Page Down (toggle yaw
  mode), plus Ctrl+Shift+Y / G / H chord equivalents. Polled at ~60 Hz on a
  background thread.
- File logger at `Yakuza0HeadTracking.log` next to the .asi.
- Camera telemetry build switch for per-fire frame-state logging during
  reverse engineering.

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
  this: input is lower latency but also less damped, and because the mod
  exposes no smoothing key there is no way to opt back into the old 0.15
  floor. That per-connection choice is only meaningful because the receiver
  binds all interfaces, which it has always done; an earlier entry here
  described it as loopback-only, which was never true.
