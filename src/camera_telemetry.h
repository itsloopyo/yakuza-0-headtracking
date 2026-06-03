#pragma once

#include <cstdint>

namespace yakuza0 {

struct CameraState;

// In-process diagnostics for the camera hook. Everything here is read-only
// with respect to the engine: it observes CameraState and writes log lines,
// never modifies game state. mod.cpp calls into it from the hook hot path.
namespace telemetry {

// Off in shipping builds: the per-fire diagnostics (mutex-guarded set
// tracking, 1Hz state/matrix logging) exist only to map camera vtables for
// the gameplay allow-list. Once a build is classified they are pure render-
// thread overhead, so mod.cpp compiles them out behind `if constexpr`. Flip
// to true (or define CUL_CAMERA_TELEMETRY=1) to re-run identification after a
// game patch shifts the vtables.
#ifndef CUL_CAMERA_TELEMETRY
#define CUL_CAMERA_TELEMETRY 0
#endif
constexpr bool kEnabled = CUL_CAMERA_TELEMETRY != 0;

// Game module base used to compute vtable RVAs. Set once by InstallCameraHook
// before the hook can fire.
void SetModuleBase(uintptr_t moduleBase);

// Module-relative RVA of the camera object's vtable, or 0 for a null camera.
// The vtable RVA is the per-type discriminator used by the gameplay-camera
// allow-list (RTTI is stripped from the retail EXE).
uintptr_t CameraVtableRva(void* cameraObj);

// Per-fire diagnostics: logs each newly seen camera vtable, tracks the
// 1s-window active-camera set ("cams:" lines), and counts hook fires.
void RecordCameraFire(uintptr_t vtRva);

// Rate-limited (1Hz) state dump: clean vs injected camera vectors, pose, and
// the engine's rebuilt view matrix. The matrix is last frame's output built
// FROM the vectors we modify, so it tracking the injected basis proves the
// engine consumed the injection - control verified from inside the process.
void LogFrameState(const CameraState* state, uintptr_t vtRva,
                   const float cleanPos[3], const float cleanFoc[3], const float cleanUp[3],
                   bool udpFresh, bool injected,
                   float poseYaw, float posePitch, float poseRoll);

}  // namespace telemetry
}  // namespace yakuza0
