# Changelog

All notable changes to this project are documented here. Dev builds are
published as a rolling `dev` pre-release and track the Unreleased section
below; a dated entry is added when a versioned release is cut.

## [Unreleased]

### Added
- Initial repo scaffold from cameraunlock-core templates (C++ ASI mod).
- Ultimate ASI Loader install/uninstall scripts.
- CMake project producing `Yakuza0HeadTracking.asi`.
- Reverse-engineering notes for the camera hook in `.lab/camera-analysis.md`
  (lifted from etra0/yakuza-freecam, byte-verified against the current
  Steam build).
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
- OpenTrack UDP receiver on `127.0.0.1:4242` via cameraunlock-core's
  `UdpReceiver`, driving `HeadTrackingSession` (processor + position
  processor) with the doctrine baseline smoothing floor (0.15).
- INI config (`Yakuza0HeadTracking.ini`, written next to the .asi) for
  `WorldSpaceYaw` and `YawModeKey`, with virtual-key-range validation.
- Hotkeys: Home (recenter), End (toggle), Page Up (cycle tracking mode),
  Page Down (toggle yaw mode), plus Ctrl+Shift+T / Y / G / H chord
  equivalents. Polled at ~60 Hz on a background thread.
- File logger at `Yakuza0HeadTracking.log` next to the .asi.
- Camera telemetry build switch for per-fire frame-state logging during
  reverse engineering.
