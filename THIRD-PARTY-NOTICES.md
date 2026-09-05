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
| MinHook | inside Ultimate ASI Loader v9.7.2 | BSD-2-Clause | Compiled into the vendored dinput8.dll |
| injector | `f7fd18f` (inside Ultimate ASI Loader v9.7.2) | zlib | Compiled into the vendored dinput8.dll |
| miniz | 3.0.0 (inside Ultimate ASI Loader v9.7.2) | MIT | Compiled into the vendored dinput8.dll |
| cameraunlock-core | 29b11b62f183183295d435b7292d8c1c0a8e5cff | MIT | Compiled into `Yakuza0HeadTracking.asi` |
| OpenTrack | n/a | ISC | Not bundled, not linked; UDP wire format only |
| etra0/yakuza-freecam | n/a | MIT | Not bundled, not linked; published engine findings only |

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

That `dinput8.dll` is a static binary and is not one component. The
`Ultimate-ASI-Loader-x64` target in `premake5.lua` at v9.7.2 compiles
`external/injector/minhook/src/**.c`,
`external/injector/utility/FunctionHookMinHook.cpp` and `external/miniz/miniz.c`
alongside the loader's own sources, so redistributing it redistributes MinHook,
injector and miniz as well, and each has its own section in this file.
MemoryModule, d3d8to9 and the minidx9 DirectX headers belong to the 32-bit
target only and are absent from this binary.

---

## MinHook

Compiled into the vendored `dinput8.dll`. Function hooking inside the loader
itself, compiled in by the `Ultimate-ASI-Loader-x64` target in `premake5.lua` at
v9.7.2. Nothing in this repository links it; it ships only inside that binary.
Its `hde32.c` / `hde64.c` are compiled in with it, so the text below reproduces
the Hacker Disassembler Engine copyright the licence file carries alongside
Tsuda Kageyu's.

- Upstream: https://github.com/TsudaKageyu/minhook
- Version: the copy compiled into Ultimate ASI Loader v9.7.2 from
  `external/injector/minhook/src/**.c`
- Licence: BSD-2-Clause

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## injector

Compiled into the vendored `dinput8.dll`. The loader's `FunctionHookMinHook`
wrapper, which the `Ultimate-ASI-Loader-x64` target compiles from
`external/injector/utility/FunctionHookMinHook.cpp`, and the MinHook submodule
that repository carries. Nothing in this repository calls or links it; it ships
only inside that binary.

- Upstream: https://github.com/ThirteenAG/injector
- Version: commit `f7fd18f7fcb4691f470b7a047697e591a39a94fc`, the submodule
  Ultimate ASI Loader v9.7.2 pins at `external/injector/`
- Licence: zlib

The binary is unaltered upstream, so the "altered source versions" condition
below does not arise. It is reproduced whole regardless.

```
Copyright (C) 2012-2014 LINK/2012 <dma_2012@hotmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
```

---

## miniz

Compiled into the vendored `dinput8.dll`. Zip reading for the loader's
`LoadVirtualFilesFromZip` path, which the `Ultimate-ASI-Loader-x64` target
compiles from `external/miniz/miniz.c`. Nothing in this repository calls or
links it; it ships only inside that binary.

- Upstream: https://github.com/richgel999/miniz
- Version: 3.0.0, as vendored at `external/miniz/` in Ultimate ASI Loader v9.7.2
- Licence: MIT

```
Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## cameraunlock-core

Git submodule at `cameraunlock-core/`, compiled into `Yakuza0HeadTracking.asi`. Our own code,
MIT licensed, reproduced here so the notices are complete.

- Pinned commit: `29b11b62f183183295d435b7292d8c1c0a8e5cff`

```
MIT License

Copyright (c) 2026 itsloopyo

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
from this project. What we took from it are the findings about Yakuza 0 that
its author published, and this section records exactly which, because the mod
would not have been written without them.

- Upstream: https://github.com/etra0/yakuza-freecam
- Licence: MIT, Copyright (c) 2020 SebastiÃƒÆ’Ã‚Â¡n A. (etra0)

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
contained in this repository or in either release ZIP. No game source of any
kind is contained here.

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
