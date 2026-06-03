#include "camera_telemetry.h"

#include "camera_hook.h"
#include "gameplay_cameras.h"
#include "logging.h"

#include <cameraunlock/time/qpc_clock.h>

#include <atomic>
#include <cstdio>
#include <mutex>

namespace yakuza0 {
namespace telemetry {

namespace {

int64_t NowUs() {
    return static_cast<int64_t>(cameraunlock::time::QpcNowMicros());
}

uintptr_t g_moduleBase = 0;

// --- First-seen vtable log ---------------------------------------------------
// Camera-type diagnostic. RTTI is stripped from the retail EXE, so a camera
// class can only be identified by its vtable RVA. The view-build virtual is
// shared by every camera type (gameplay, battle, event/cutscene), so logging
// the distinct vtables that pass through the hook - with a timestamp - lets a
// single playthrough reveal which vtable is active during a cutscene/Heat
// action/menu. That mapping is what the gameplay-camera allow-list keys on.
constexpr int kMaxSeenVtables = 32;
std::atomic<int> g_seenVtableCount{0};
uintptr_t        g_seenVtables[kMaxSeenVtables] = {};
std::mutex       g_vtableMutex;
std::atomic<int64_t> g_firstHookUs{0};

void LogNewCameraVtable(uintptr_t vtRva) {
    int n = g_seenVtableCount.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        if (g_seenVtables[i] == vtRva) return;

    std::lock_guard<std::mutex> lock(g_vtableMutex);
    n = g_seenVtableCount.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
        if (g_seenVtables[i] == vtRva) return;
    if (n >= kMaxSeenVtables) return;

    const int64_t nowUs = NowUs();
    int64_t firstUs = 0;
    if (g_firstHookUs.compare_exchange_strong(firstUs, nowUs, std::memory_order_relaxed))
        firstUs = nowUs;
    const long long elapsedMs = (nowUs - firstUs) / 1000;

    g_seenVtables[n] = vtRva;
    g_seenVtableCount.store(n + 1, std::memory_order_release);
    log::Line("camera_vtable: type #%d vtable RVA 0x%llX (+%lldms)",
              n, (unsigned long long)vtRva, elapsedMs);
}

// --- Active-camera-set log ---------------------------------------------------
// Logs a "cams:" line whenever the set of camera vtables firing the hook
// changes within a 1s window, and a gap line when firing resumes after a
// suspension (pause / loading / focus loss). Scene transitions - gameplay <->
// cutscene <-> menu - show up as set changes; this is the data the
// gameplay-camera allow-list gate is built from. Allowed (gameplay) vtables
// are marked with '*' in the line.
constexpr int kMaxWindowVtables = 16;
constexpr int64_t kWindowLengthUs = 1000000;
constexpr int64_t kFiringGapUs = 3000000;
uintptr_t   g_windowVts[kMaxWindowVtables] = {};
uint32_t    g_windowFires[kMaxWindowVtables] = {};
int         g_windowCount = 0;
uintptr_t   g_prevSetVts[kMaxWindowVtables] = {};
int         g_prevSetCount = 0;
int64_t     g_windowStartUs = 0;
std::mutex  g_windowMutex;

void TrackCameraActivity(uintptr_t vtRva) {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    const int64_t nowUs = NowUs();
    if (g_windowStartUs == 0) g_windowStartUs = nowUs;

    int i = 0;
    for (; i < g_windowCount; ++i)
        if (g_windowVts[i] == vtRva) { g_windowFires[i]++; break; }
    if (i == g_windowCount && g_windowCount < kMaxWindowVtables) {
        g_windowVts[i] = vtRva;
        g_windowFires[i] = 1;
        ++g_windowCount;
    }

    const int64_t elapsedUs = nowUs - g_windowStartUs;
    if (elapsedUs < kWindowLengthUs) return;

    if (elapsedUs > kFiringGapUs)
        log::Line("cams: %.1fs firing gap", elapsedUs / 1e6);

    bool changed = (g_windowCount != g_prevSetCount);
    for (int j = 0; j < g_windowCount && !changed; ++j) {
        bool inPrev = false;
        for (int k = 0; k < g_prevSetCount; ++k)
            if (g_prevSetVts[k] == g_windowVts[j]) { inPrev = true; break; }
        changed = !inPrev;
    }

    if (changed) {
        char buf[256];
        int off = 0;
        for (int j = 0; j < g_windowCount && off < static_cast<int>(sizeof(buf)) - 32; ++j)
            off += snprintf(buf + off, sizeof(buf) - off, "%s0x%llX%s(%u)",
                            j ? " " : "", (unsigned long long)g_windowVts[j],
                            IsGameplayCamera(g_windowVts[j]) ? "*" : "", g_windowFires[j]);
        log::Line("cams: %s", buf);
    }

    for (int j = 0; j < g_windowCount; ++j) g_prevSetVts[j] = g_windowVts[j];
    g_prevSetCount = g_windowCount;
    g_windowCount = 0;
    g_windowStartUs = nowUs;
}

// --- 1Hz state dump ----------------------------------------------------------
std::atomic<uint64_t> g_hookFires{0};
std::atomic<int64_t>  g_lastTelemetryUs{0};

// The engine rebuilds the view matrix at this offset inside the camera object
// every frame, FROM the spilled focus/up/position vectors we modify.
constexpr ptrdiff_t kViewMatrixOffset = 0x110;
constexpr int64_t kTelemetryIntervalUs = 1000000;

}  // namespace

void SetModuleBase(uintptr_t moduleBase) {
    g_moduleBase = moduleBase;
}

uintptr_t CameraVtableRva(void* cameraObj) {
    if (!cameraObj) return 0;
    const uintptr_t vt = *reinterpret_cast<uintptr_t*>(cameraObj);
    return vt - g_moduleBase;
}

void RecordCameraFire(uintptr_t vtRva) {
    g_hookFires.fetch_add(1, std::memory_order_relaxed);
    if (!vtRva) return;
    LogNewCameraVtable(vtRva);
    TrackCameraActivity(vtRva);
}

void LogFrameState(const CameraState* state, uintptr_t vtRva,
                   const float cleanPos[3], const float cleanFoc[3], const float cleanUp[3],
                   bool udpFresh, bool injected,
                   float poseYaw, float posePitch, float poseRoll) {
    const int64_t nowUs = NowUs();
    int64_t last = g_lastTelemetryUs.load(std::memory_order_relaxed);
    if (nowUs - last < kTelemetryIntervalUs) return;
    if (!g_lastTelemetryUs.compare_exchange_strong(last, nowUs, std::memory_order_relaxed)) return;

    log::Line("cam: fires=%llu obj=%p vt=0x%llX pos(%.3f,%.3f,%.3f) cfoc(%.3f,%.3f,%.3f) cup(%.3f,%.3f,%.3f) fov=%.4f udp=%d inj=%d",
              (unsigned long long)g_hookFires.load(std::memory_order_relaxed),
              state->cameraObj, (unsigned long long)vtRva,
              cleanPos[0], cleanPos[1], cleanPos[2],
              cleanFoc[0], cleanFoc[1], cleanFoc[2],
              cleanUp[0], cleanUp[1], cleanUp[2],
              state->fov, udpFresh ? 1 : 0, injected ? 1 : 0);

    if (injected) {
        log::Line("inj: pose(%.2f,%.2f,%.2f) ipos(%.3f,%.3f,%.3f) ifoc(%.3f,%.3f,%.3f) iup(%.3f,%.3f,%.3f)",
                  poseYaw, posePitch, poseRoll,
                  state->position[0], state->position[1], state->position[2],
                  state->focus[0], state->focus[1], state->focus[2],
                  state->up[0], state->up[1], state->up[2]);
    }

    if (state->cameraObj) {
        const float* m = reinterpret_cast<const float*>(
            static_cast<const uint8_t*>(state->cameraObj) + kViewMatrixOffset);
        log::Line("mtx: %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f | %.4f %.4f %.4f %.4f",
                  m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                  m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
    }
}

}  // namespace telemetry
}  // namespace yakuza0
