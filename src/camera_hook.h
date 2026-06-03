#pragma once

#include <cstdint>

namespace yakuza0 {

// Layout matches the etra0/yakuza-freecam interceptor buffer layout for the
// hook at RVA 0x18FD38. xmm5 -> +0x00 (focus), xmm6 -> +0x20 (position),
// xmm4 -> +0x40 (up). FOV at +0x60 is the 8-byte slot at [rax+0xAC] read
// at the hook site; only the low 4 bytes are the actual float FOV.
struct alignas(16) CameraState {
    float    focus[4];     // +0x00 - lookAt target (xmm5)
    float    _pad_focus[4];// +0x10
    float    position[4];  // +0x20 - eye position (xmm6)
    float    _pad_pos[4];  // +0x30
    float    up[4];        // +0x40 - up vector (xmm4)
    float    _pad_up[4];   // +0x50
    float    fov;          // +0x60 - radians
    uint32_t _pad_fov[3];  // +0x64
    void*    cameraObj;    // +0x70 - rbx at the hook site (the camera instance).
                           // *cameraObj is its vtable pointer: a per-type
                           // discriminator (gameplay vs cutscene/event cam)
                           // that gates injection (IsGameplayCamera) and feeds
                           // the camera-type telemetry. The engine never reads
                           // this slot.
};

// Patches the camera state spill site and routes it through the asm
// trampoline into ApplyTrackingToCamera. Returns false (and leaves the game
// untouched) if the pattern is missing, ambiguous, or cannot be patched
// safely. The hook stays installed for the lifetime of the process.
bool InstallCameraHook();

}  // namespace yakuza0
