#include "mod.h"

#include "camera_hook.h"
#include "camera_telemetry.h"
#include "gameplay_cameras.h"
#include "logging.h"
#include "mod_config.h"
#include "view_math.h"

#include <cameraunlock/config/defaults_file.h>
#include <cameraunlock/input/chord_hotkeys.h>
#include <cameraunlock/input/hotkey_poller.h>
#include <cameraunlock/input/key_binding_registration.h>
#include <cameraunlock/input/key_bindings.h>
#include <cameraunlock/protocol/udp_receiver.h>
#include <cameraunlock/time/frame_clock.h>
#include <cameraunlock/tracking/head_tracking_session.h>

#include <windows.h>

#include <atomic>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace yakuza0 {

namespace {

using cameraunlock::HeadTrackingSession;
using cameraunlock::TrackingMode;
using cameraunlock::UdpReceiver;
using cameraunlock::math::Vec3;

UdpReceiver                       g_receiver;
HeadTrackingSession<UdpReceiver>  g_session{g_receiver};
// The receiver's connection classification is the only thing that selects
// between LocalSmoothing and RemoteSmoothing. Without IsRemoteConnection() the
// session silently reports every tracker as local and pins remote users to
// LocalSmoothing forever, with nothing at the call site to show it.
static_assert(decltype(g_session)::kHasRemoteConnection,
              "receiver must expose IsRemoteConnection() or remote smoothing never applies");
cameraunlock::time::FrameClock    g_clock;
cameraunlock::input::HotkeyPoller g_hotkeys;

std::atomic<bool> g_modEnabled{true};

// Yaw application mode; see ApplyHeadPose in view_math.h for the two modes.
std::atomic<bool> g_worldSpaceYaw{true};

std::vector<cameraunlock::input::KeyBinding> Bindings(const char* key, const std::string& list) {
    const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(list);
    if (!parsed.ok()) throw std::logic_error(std::string(key) + "='" + list + "': " + parsed.error);
    return parsed.bindings;
}

void RegisterHotkeys(const Config& cfg) {
    using cameraunlock::input::ChordGuarded;
    using cameraunlock::input::RegisterKeyBindings;

    // End changes this session only; EnableOnStartup decides the next start.
    auto toggle = [] {
        const bool now = !g_modEnabled.load(std::memory_order_relaxed);
        g_modEnabled.store(now, std::memory_order_relaxed);
        log::Line("hotkey: tracking %s", now ? "enabled" : "disabled");
    };
    auto cycleMode = [] {
        const TrackingMode mode = g_session.CycleMode();
        const char* name = "";
        switch (mode) {
            case TrackingMode::RotationAndPosition: name = "rotation + position"; break;
            case TrackingMode::RotationOnly:        name = "rotation only";       break;
            case TrackingMode::PositionOnly:        name = "position only";       break;
        }
        log::Line("hotkey: tracking mode -> %s", name);
        config::SaveTrackingMode(mode);
    };
    auto toggleYawMode = [] {
        const bool now = !g_worldSpaceYaw.load(std::memory_order_relaxed);
        g_worldSpaceYaw.store(now, std::memory_order_relaxed);
        log::Line("hotkey: yaw mode -> %s", now ? "world-space (horizon-locked)" : "camera-local");
        config::SaveWorldSpaceYaw(now);
    };

    // Each list holds every key that fires its action, the Ctrl+Shift chord
    // included. A key without modifiers stays silent while Ctrl and Shift are
    // both held, so Ctrl+Shift+<key> reaches only a binding that names the chord.
    RegisterKeyBindings(g_hotkeys, Bindings("ToggleKey", cfg.toggle_key), toggle);
    RegisterKeyBindings(g_hotkeys, Bindings("CycleTrackingModeKey", cfg.cycle_tracking_mode_key), cycleMode);
    RegisterKeyBindings(g_hotkeys, Bindings("YawModeKey", cfg.yaw_mode_key), toggleYawMode);
    log::Line("hotkey: toggle=[%s] cycle tracking mode=[%s] yaw mode=[%s]", cfg.toggle_key.c_str(),
              cfg.cycle_tracking_mode_key.c_str(), cfg.yaw_mode_key.c_str());

    if constexpr (telemetry::kEnabled) {
        g_hotkeys.AddHotkey('U', ChordGuarded([] { telemetry::ResetMatrixDumps(); }));
    }
}

void InitThread() {
    // Keep one previous generation: the session worth diagnosing is usually the
    // one that just crashed, and the user relaunches the game before sending it.
    DWORD rotateError = 0;
    // Named, not temporaries in the condition: they are destroyed at the end of
    // the if-condition, so the deallocation runs before GetLastError() below.
    const std::wstring logPathW = LogFilePath();
    const std::wstring prevPathW = PrevLogFilePath();
    if (!MoveFileExW(logPathW.c_str(), prevPathW.c_str(),
                     MOVEFILE_REPLACE_EXISTING)) {
        rotateError = GetLastError();
    }
    log::Open(LogFilePath());
    log::Line("Yakuza0HeadTracking 0.0.0 starting up");
    // The open above truncates, so a failed rotation has already destroyed the
    // generation the rotation existed to keep. Say so instead of losing it
    // silently. A missing source file is the ordinary first-run case.
    if (rotateError != 0 && rotateError != ERROR_FILE_NOT_FOUND) {
        log::Line("init: could not rotate the previous log (error %lu) - the previous session's log was overwritten",
                  rotateError);
    }

    const Config cfg = config::Load(ModuleDir(), cameraunlock::config::DefaultsFile::PerUser());
    g_modEnabled.store(cfg.enable_on_startup, std::memory_order_relaxed);
    g_worldSpaceYaw.store(cfg.world_space_yaw, std::memory_order_relaxed);

    if (!InstallCameraHook()) {
        log::Line("init: camera hook install failed; tracking disabled");
        return;
    }

    // Rotation via focus/up rewrite, position via a camera-position offset
    // applied in the clean (pre-rotation) camera basis.
    g_session.SetMode(config::StartupTrackingMode(cfg));
    g_session.SetLocalSmoothing(cfg.local_smoothing);
    g_session.SetRemoteSmoothing(cfg.remote_smoothing);

    // OpenTrack pitch-up reads as look-down in the engine's lookAt basis.
    cameraunlock::SensitivitySettings sensitivity;
    sensitivity.invert_pitch = true;
    g_session.GetProcessor().SetSensitivity(sensitivity);

    // Tracker +X (lean right) reads as the opposite direction in the engine's
    // lookAt basis. Its +Z runs the other way too, which InjectTracking flips
    // after the processor has clamped z in the pipeline's own convention.
    cameraunlock::PositionSettings position;
    position.invert_x = true;
    position.limit_x = cfg.position_limit_x;
    position.limit_y = cfg.position_limit_y;
    position.limit_y_down = cfg.position_limit_y_down;
    position.limit_z = cfg.position_limit_z;
    position.limit_z_back = cfg.position_limit_z_back;
    g_session.SetPositionSettings(position);

    g_receiver.SetLog([](const std::string& s) { log::Line("udp: %s", s.c_str()); });
    if (!g_receiver.Start(static_cast<uint16_t>(cfg.udp_port))) {
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
        // The engine's lookAt basis has +z forward; the pipeline's forward lean
        // is -z.
        const Vec3 posOffset = basis.LocalToWorld(Vec3(px, py, -pz));
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
