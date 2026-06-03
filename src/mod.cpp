#include "mod.h"

#include "camera_hook.h"
#include "camera_telemetry.h"
#include "gameplay_cameras.h"
#include "logging.h"
#include "mod_config.h"
#include "view_math.h"

#include <cameraunlock/input/chord_hotkeys.h>
#include <cameraunlock/input/hotkey_poller.h>
#include <cameraunlock/protocol/udp_receiver.h>
#include <cameraunlock/time/frame_clock.h>
#include <cameraunlock/tracking/head_tracking_session.h>

#include <windows.h>

#include <atomic>
#include <exception>
#include <string>
#include <thread>

namespace yakuza0 {

namespace {

using cameraunlock::HeadTrackingSession;
using cameraunlock::TrackingMode;
using cameraunlock::UdpReceiver;
using cameraunlock::math::Vec3;

UdpReceiver                       g_receiver;
HeadTrackingSession<UdpReceiver>  g_session{g_receiver};
cameraunlock::time::FrameClock    g_clock;
cameraunlock::input::HotkeyPoller g_hotkeys;

std::atomic<bool> g_modEnabled{true};
std::atomic<bool> g_recenterRequested{false};

// Yaw application mode; see ApplyHeadPose in view_math.h for the two modes.
std::atomic<bool> g_worldSpaceYaw{true};

void RegisterHotkeys(const Config& cfg) {
    using cameraunlock::input::ChordGuarded;
    using cameraunlock::input::NavGuarded;

    auto recenter = [] {
        g_recenterRequested.store(true, std::memory_order_release);
        log::Line("hotkey: recenter requested");
    };
    auto toggle = [] {
        const bool now = !g_modEnabled.load(std::memory_order_relaxed);
        g_modEnabled.store(now, std::memory_order_relaxed);
        log::Line("hotkey: tracking %s", now ? "enabled" : "disabled");
    };
    auto cycleMode = [] {
        const char* name = "";
        switch (g_session.CycleMode()) {
            case TrackingMode::RotationAndPosition: name = "rotation + position"; break;
            case TrackingMode::RotationOnly:        name = "rotation only";       break;
            case TrackingMode::PositionOnly:        name = "position only";       break;
        }
        log::Line("hotkey: tracking mode -> %s", name);
    };
    auto toggleYawMode = [] {
        const bool now = !g_worldSpaceYaw.load(std::memory_order_relaxed);
        g_worldSpaceYaw.store(now, std::memory_order_relaxed);
        log::Line("hotkey: yaw mode -> %s", now ? "world-space (horizon-locked)" : "camera-local");
    };

    g_hotkeys.SetRecenterKey(VK_HOME, NavGuarded(recenter));
    g_hotkeys.SetToggleKey(VK_END, NavGuarded(toggle));
    g_hotkeys.AddHotkey(VK_PRIOR, NavGuarded(cycleMode));
    g_hotkeys.AddHotkey(cfg.yawModeKey, NavGuarded(toggleYawMode));
    g_hotkeys.AddHotkey('T', ChordGuarded(recenter));
    g_hotkeys.AddHotkey('Y', ChordGuarded(toggle));
    g_hotkeys.AddHotkey('G', ChordGuarded(cycleMode));
    g_hotkeys.AddHotkey('H', ChordGuarded(toggleYawMode));
}

void InitThread() {
    log::Open(LogFilePath());
    log::Line("Yakuza0HeadTracking 0.0.0 starting up");

    const Config cfg = LoadConfig();
    g_worldSpaceYaw.store(cfg.worldSpaceYaw, std::memory_order_relaxed);

    if (!InstallCameraHook()) {
        log::Line("init: camera hook install failed; tracking disabled");
        return;
    }

    // 6DOF: rotation via focus/up rewrite, position via a camera-position
    // offset applied in the clean (pre-rotation) camera basis.
    g_session.SetMode(TrackingMode::RotationAndPosition);

    // OpenTrack pitch-up reads as look-down in the engine's lookAt basis.
    cameraunlock::SensitivitySettings sensitivity;
    sensitivity.invert_pitch = true;
    g_session.GetProcessor().SetSensitivity(sensitivity);

    // Tracker +X (lean right) and +Z (lean forward) read as the opposite
    // directions in the engine's lookAt basis. The asymmetric Z clamp is
    // applied after the inversion, so swap its bounds to keep the generous
    // range on the forward lean and the restricted range on the backward.
    cameraunlock::PositionSettings position;
    position.invert_x = true;
    position.invert_z = true;
    position.limit_z = 0.10f;
    position.limit_z_back = 0.40f;
    g_session.GetPositionProcessor().SetSettings(position);

    g_receiver.SetLog([](const std::string& s) { log::Line("udp: %s", s.c_str()); });
    if (!g_receiver.Start()) {
        log::Line("init: UDP receiver Start() did not bind immediately (retry thread may be running)");
    }

    RegisterHotkeys(cfg);
    if (!g_hotkeys.Start()) {
        log::Line("init: hotkey poller failed to start");
    }

    log::Line("init: complete");
}

// Computes the head-tracked camera vectors and writes them into the engine's
// CameraState. Returns false (state untouched) when tracking should not or
// cannot be applied this frame.
bool InjectTracking(CameraState* state, float& poseYaw, float& posePitch, float& poseRoll,
                    bool& udpFresh) {
    if (g_recenterRequested.exchange(false, std::memory_order_acq_rel)) {
        g_session.Recenter();
        log::Line("recentered");
    }

    if (!g_session.Update(g_clock.Tick())) return false;
    udpFresh = true;
    if (!g_session.GetRotation(poseYaw, posePitch, poseRoll)) return false;

    CameraBasis basis;
    if (!BuildCameraBasis(Vec3(state->position[0], state->position[1], state->position[2]),
                          Vec3(state->focus[0],    state->focus[1],    state->focus[2]),
                          Vec3(state->up[0],       state->up[1],       state->up[2]),
                          basis)) {
        return false;
    }

    const TrackedOrientation tracked = ApplyHeadPose(
        basis, poseYaw, posePitch, poseRoll,
        g_worldSpaceYaw.load(std::memory_order_relaxed));

    const Vec3 pos(state->position[0], state->position[1], state->position[2]);
    Vec3 newFoc = pos + tracked.forward * basis.focalDistance;

    // 6DOF: pure translation in the clean (pre-rotation) camera basis so
    // the offset follows body orientation, not the head-rotated view.
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    if (g_session.GetPositionOffset(px, py, pz)) {
        const Vec3 posOffset = basis.LocalToWorld(Vec3(px, py, pz));
        const Vec3 newPos = pos + posOffset;
        newFoc = newFoc + posOffset;
        state->position[0] = newPos.x;
        state->position[1] = newPos.y;
        state->position[2] = newPos.z;
    }

    state->focus[0] = newFoc.x;
    state->focus[1] = newFoc.y;
    state->focus[2] = newFoc.z;
    state->up[0] = tracked.up.x;
    state->up[1] = tracked.up.y;
    state->up[2] = tracked.up.z;
    return true;
}

}  // namespace

void StartModAsync() {
    std::thread([] {
        // This runs on a detached thread inside the game process. An
        // uncaught exception here would call std::terminate and crash the
        // game on startup; tracking simply being unavailable is the correct
        // failure mode for a cosmetic mod, so contain it to the log.
        try {
            InitThread();
        } catch (const std::exception& e) {
            log::Line("init: fatal exception, tracking disabled: %s", e.what());
        } catch (...) {
            log::Line("init: fatal unknown exception, tracking disabled");
        }
    }).detach();
}

void ApplyTrackingToCamera(CameraState* state) {
    if (!state) return;
    const uintptr_t vtRva = telemetry::CameraVtableRva(state->cameraObj);
    const bool apply = g_modEnabled.load(std::memory_order_relaxed) && IsGameplayCamera(vtRva);

    if constexpr (!telemetry::kEnabled) {
        if (apply) {
            float poseYaw, posePitch, poseRoll;
            bool udpFresh;
            InjectTracking(state, poseYaw, posePitch, poseRoll, udpFresh);
        }
        return;
    }

    telemetry::RecordCameraFire(vtRva);

    const float cleanPos[3] = { state->position[0], state->position[1], state->position[2] };
    const float cleanFoc[3] = { state->focus[0], state->focus[1], state->focus[2] };
    const float cleanUp[3]  = { state->up[0],    state->up[1],    state->up[2] };

    bool udpFresh = false;
    bool injected = false;
    float poseYaw = 0.0f, posePitch = 0.0f, poseRoll = 0.0f;

    if (apply) {
        injected = InjectTracking(state, poseYaw, posePitch, poseRoll, udpFresh);
    }

    telemetry::LogFrameState(state, vtRva, cleanPos, cleanFoc, cleanUp,
                             udpFresh, injected, poseYaw, posePitch, poseRoll);
}

}  // namespace yakuza0
