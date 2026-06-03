#pragma once

namespace yakuza0 {

struct CameraState;

// Started from DllMain on PROCESS_ATTACH. Spawns a background init thread.
void StartModAsync();

// Called from the camera hook (asm shim -> camera_hook.cpp -> here).
// Replaces state->focus, state->up, and state->position with head-tracked
// equivalents when tracking is active and the firing camera is a gameplay
// camera; otherwise leaves the state untouched.
void ApplyTrackingToCamera(CameraState* state);

}  // namespace yakuza0
