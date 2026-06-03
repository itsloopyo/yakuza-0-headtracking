#pragma once

#include <cstdint>

namespace yakuza0 {

// Gameplay-camera allow-list. Tracking is injected only when the firing
// camera's vtable is in this list; every other camera type (cutscene,
// dialogue, pause menu, loading, title, minigame) gets clean passthrough -
// fail-safe per doctrine: unknown camera types mean no injection. RTTI is
// stripped from the retail EXE so cameras are identified by vtable RVA,
// mapped empirically via the cams:/camera_vtable telemetry in
// camera_telemetry.cpp. RVAs are per-build (Steam build of 2025-05-04, EXE
// size 20,156,624); a game patch shifts them, at which point new values are
// appended - never edited in place.
//
// Identified so far (sessions 2026-06-03, see .lab/camera-analysis.md):
//   0xD36690  free-roam follow camera + street battles -> allow
//   0xD37340  first-person view camera       -> allow
//   0xD36CF0  pause menu camera              -> block
//   0xE21AC0  dialogue/conversation camera   -> block
//   0xD44200  cinematic cutscene + Heat-action cinematic + title -> block
//   0xD79E80  Heat-action transition camera  -> block
//   0xD1D440  loading screen                 -> block
//   0xD50860  boot                           -> block
//   0xD360C0  scene-transition               -> block
//   Minigame cameras not yet observed - append when identified.
constexpr uintptr_t kGameplayCameraVtables[] = {
    0xD36690,  // free-roam follow camera + street battles
    0xD37340,  // first-person view camera (player-controlled free look)
};

inline bool IsGameplayCamera(uintptr_t vtRva) {
    for (uintptr_t v : kGameplayCameraVtables)
        if (v == vtRva) return true;
    return false;
}

}  // namespace yakuza0
