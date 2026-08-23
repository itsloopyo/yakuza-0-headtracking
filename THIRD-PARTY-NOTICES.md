# Third-Party Notices

Yakuza0HeadTracking bundles, statically links, or credits the third-party components
listed below. Each remains the property of its authors and is used under its own
licence. Where a licence requires the copyright notice, the conditions and the
disclaimer to accompany a binary distribution, the full text is reproduced here
verbatim, and this file ships at the root of every release ZIP we publish.

This repository contains no Yakuza 0 code, no extracted Yakuza 0 assets and no
Yakuza 0 data files, and neither release ZIP distributes any. What the source
does record about the game is described under "Yakuza 0" below.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| Ultimate ASI Loader | v9.7.2 | MIT | Bundled verbatim in the installer ZIP |
| cameraunlock-core | 3465659888b2270addac9de0b2a728f59a00360c | MIT | Compiled into `Yakuza0HeadTracking.asi` |
| OpenTrack | n/a | ISC | Not bundled, not linked; UDP wire format only |
| etra0/yakuza-freecam | n/a | MIT | Not bundled, not linked; reverse-engineering findings only |

---

## Ultimate ASI Loader

Vendored at `vendor/ultimate-asi-loader/`, shipped in the installer ZIP and used as the
install-time source. Taken from the upstream release asset untouched; the
upstream licence file ships beside it at `vendor/ultimate-asi-loader/LICENSE`.

- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader
- Version: `v9.7.2`
- Commit: `ab722befd52581a34449b603926cfab476e66b05`
- SHA-256: `22fda9c71eaae02460f311bf3441638340ab591586d78f1de213c4819dcb883c`

```
MIT License

Copyright (c) 2023 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## cameraunlock-core

Git submodule at `cameraunlock-core/`, compiled into `Yakuza0HeadTracking.asi`. Our own code,
MIT licensed, reproduced here so the notices are complete.

- Pinned commit: `3465659888b2270addac9de0b2a728f59a00360c`

```
MIT License

Copyright (c) 2026 CameraUnlock

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## OpenTrack

Not bundled and not linked. This mod implements the OpenTrack UDP pose datagram
layout so that OpenTrack (https://github.com/opentrack/opentrack, ISC licence)
and compatible trackers can drive it. No OpenTrack code, headers or binaries
are copied, linked or redistributed, so its licence triggers no notice
obligation here. It is credited because the wire format is its work.

---

## etra0/yakuza-freecam

Not bundled and not linked. No source code, in any language, has been copied
from this project. What we took from it are reverse-engineering findings about
Yakuza 0 that its author published, and this section records exactly which,
because the mod would not have been written without them.

- Upstream: https://github.com/etra0/yakuza-freecam
- Licence: MIT, Copyright (c) 2020 Sebastián A. (etra0)

Taken from their published work:

- The camera hook site at RVA `0x18FD38`, and that a 5-byte detour is what fits
  there (`yakuza0/src/main.rs`).
- The register meaning at that site and the resulting interceptor buffer
  layout, mirrored by `CameraState` in `src/camera_hook.h`: focus in `xmm5` at
  `+0x00`, position in `xmm6` at `+0x20`, up in `xmm4` at `+0x40`, and the FOV
  read as the 8-byte slot at `[rax+0xAC]` into `+0x60`
  (`yakuza0/src/interceptor.asm`).

Not taken from them, contrary to what an earlier revision of this file said:

- The byte signature in `src/camera_hook.cpp`. It appears nowhere in their
  repository, which pins the raw RVA instead. We read it off our own copy of
  the game at the address they published, so the mod survives a patch that
  moves the function.
- The trampoline. Theirs injects shellcode into the target process and anchors
  off `lea r11, [get_camera_data+200h]`; ours is a near-page absolute-jmp thunk
  into a MASM trampoline with different register discipline, thread-suspension
  patching and an ambiguity failsafe. Different code, written here.
- The camera-type allow-list, telemetry, tracking pipeline and everything else
  in `src/`.

Offsets and register assignments are factual observations about a binary
rather than expressive work, so no MIT obligation is triggered and their
licence text is not reproduced here. The credit stands because the findings
were theirs and finding them again would have cost weeks.

---

## Yakuza 0

Yakuza 0 and all related names, logos, characters and marks are trademarks of
their respective owners. They are used here only to identify the game this mod
applies to, which is nominative use and not a claim of any right in them. This
project is an unofficial, fan-made modification. It is not affiliated with,
endorsed by, or sponsored by the game's developers, its publishers, its engine
vendor, or any other rights holder. It requires a legitimately purchased copy
of the game.

No game code, extracted game assets, game data files or proprietary DLLs are
contained in this repository or in either release ZIP. Nothing here is
decompiled or disassembled game source.

Being precise about what the source does record, since a blanket claim would
be too broad:

- Function addresses, struct field offsets and camera vtable RVAs
  (`src/gameplay_cameras.h`, `src/camera_hook.h`) are factual measurements of
  a binary, recorded as numbers.
- `kCameraPattern` in `src/camera_hook.cpp` is a 16-byte search string
  matching three consecutive register-spill instructions
  (`movaps [rsp+0x40], xmm4` and the two that follow it) plus one anchor byte.
  It exists solely so the mod can locate its hook site at runtime instead of
  pinning an address that a patch would move. It is used only as a search key,
  is never executed, and carries no game logic.
- Comments in `camera_hook.asm` and `src/camera_hook.h` describe the register
  state and calling contract at that one site, in our own words, so the hook is
  maintainable. They transcribe no game function.

The only files the mod adds to a game install are its own: the ASI loader proxy
DLL, `Yakuza0HeadTracking.asi`, and the INI and log it writes beside them. It
patches no game file on disk, and modifies the camera only in process memory at
runtime. `uninstall.cmd` removes all of it and restores anything it displaced.
If a rights holder would prefer any of the above removed, open an issue and it
will be taken down.
