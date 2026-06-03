#include "camera_hook.h"

#include "camera_telemetry.h"
#include "logging.h"
#include "mod.h"

#include <cameraunlock/memory/pattern_scanner.h>

#include <windows.h>
#include <tlhelp32.h>

#include <cstring>
#include <vector>

// Symbols defined in camera_hook.asm
extern "C" {
    extern void* cul_camera_buffer;
    extern void* cul_camera_resume;
    extern void  cul_camera_trampoline();
}

namespace yakuza0 {

namespace {

CameraState g_cameraState{};
void*       g_nearThunk = nullptr;

constexpr const char* kCameraPattern =
    "0F 29 64 24 40 0F 29 6C 24 50 0F 29 74 24 60 4C";

// Length of the relative-jmp patch (E9 + rel32) and of the displaced
// instruction it overwrites (movaps [rsp+0x40], xmm4).
constexpr size_t kPatchSize = 5;

// FF 25 00 00 00 00 + 8-byte absolute target.
constexpr size_t kThunkSize = 14;
constexpr size_t kThunkPageSize = 0x1000;

// Allocate an executable page within +/- 2 GB of `near` so that a relative
// E9 jmp encoded at `near` can reach it. The page holds a 14-byte absolute
// jmp thunk to our actual trampoline (which lives at whatever address the
// loader placed our DLL at -- often more than 2 GB from the game module).
void* AllocateNear(uintptr_t target, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity;

    constexpr uintptr_t kMaxRange = 0x70000000ull;  // ~1.75 GB, leaves margin
    uintptr_t minAddr = (target > kMaxRange) ? target - kMaxRange : gran;
    uintptr_t maxAddr = target + kMaxRange;

    minAddr = (minAddr + gran - 1) & ~(uintptr_t)(gran - 1);

    for (uintptr_t addr = minAddr; addr < maxAddr; addr += gran) {
        void* p = VirtualAlloc(reinterpret_cast<LPVOID>(addr), size,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (p) return p;
    }
    return nullptr;
}

void* BuildNearThunk(uintptr_t target, void* dest) {
    void* page = AllocateNear(target, kThunkPageSize);
    if (!page) return nullptr;

    // FF 25 00 00 00 00 <abs8>   == jmp qword ptr [rip+0]; absolute target
    uint8_t* p = static_cast<uint8_t*>(page);
    p[0] = 0xFF;
    p[1] = 0x25;
    p[2] = p[3] = p[4] = p[5] = 0;
    uint64_t abs = reinterpret_cast<uint64_t>(dest);
    std::memcpy(p + 6, &abs, sizeof(abs));

    // Drop write access once the thunk is written. Leaving the page RWX for
    // the process lifetime hands any code in the process a ready-made
    // staging page and trips anti-cheat/AV W^X heuristics.
    DWORD oldProt = 0;
    if (!VirtualProtect(page, kThunkPageSize, PAGE_EXECUTE_READ, &oldProt)) {
        VirtualFree(page, 0, MEM_RELEASE);
        return nullptr;
    }
    FlushInstructionCache(GetCurrentProcess(), page, kThunkSize);
    return page;
}

enum class PatchResult { Ok, ThreadInPatchWindow, Failed };

// The game's threads execute the hook site every frame, so the 5-byte rewrite
// must never be observable half-done: a torn instruction fetch executes
// garbage. Suspend every other thread, verify none is stopped inside the
// window being rewritten, write, flush, resume - the same discipline
// MinHook/Detours apply.
PatchResult PatchCodeWithThreadsSuspended(uintptr_t addr, const uint8_t* bytes, size_t len) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return PatchResult::Failed;

    const DWORD pid = GetCurrentProcessId();
    const DWORD tid = GetCurrentThreadId();

    // Collect target thread IDs BEFORE suspending anything: pushing into the
    // vector can allocate, and allocating while a thread is suspended that
    // happens to hold the CRT heap lock would deadlock. The handle list is
    // sized up front for the same reason - no allocation inside the suspend
    // window.
    std::vector<DWORD> targets;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != tid)
                targets.push_back(te.th32ThreadID);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);

    std::vector<HANDLE> suspended;
    suspended.reserve(targets.size());
    for (DWORD t : targets) {
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, t);
        if (!h) continue;
        if (SuspendThread(h) == static_cast<DWORD>(-1)) {
            CloseHandle(h);
            continue;
        }
        suspended.push_back(h);
    }

    PatchResult result = PatchResult::Ok;
    for (HANDLE h : suspended) {
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(h, &ctx) && ctx.Rip >= addr && ctx.Rip < addr + len) {
            result = PatchResult::ThreadInPatchWindow;
            break;
        }
    }

    if (result == PatchResult::Ok) {
        DWORD oldProt = 0;
        if (VirtualProtect(reinterpret_cast<LPVOID>(addr), len, PAGE_EXECUTE_READWRITE, &oldProt)) {
            std::memcpy(reinterpret_cast<void*>(addr), bytes, len);
            DWORD restored = 0;
            VirtualProtect(reinterpret_cast<LPVOID>(addr), len, oldProt, &restored);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPCVOID>(addr), len);
        } else {
            result = PatchResult::Failed;
        }
    }

    for (HANDLE h : suspended) {
        ResumeThread(h);
        CloseHandle(h);
    }
    return result;
}

PatchResult PatchCodeWithRetries(uintptr_t addr, const uint8_t* bytes, size_t len) {
    // A thread parked exactly inside the 5-byte window is transient; retry a
    // few times before giving up and staying dormant.
    for (int attempt = 0; attempt < 5; ++attempt) {
        const PatchResult r = PatchCodeWithThreadsSuspended(addr, bytes, len);
        if (r != PatchResult::ThreadInPatchWindow) return r;
        Sleep(10);
    }
    return PatchResult::ThreadInPatchWindow;
}

}  // namespace

bool InstallCameraHook() {
    HMODULE hMod = GetModuleHandleW(L"Yakuza0.exe");
    if (!hMod) hMod = GetModuleHandleW(nullptr);
    if (!hMod) {
        log::Line("camera_hook: GetModuleHandle failed");
        return false;
    }

    uintptr_t moduleBase = 0;
    size_t moduleSize = 0;
    if (!cameraunlock::memory::GetModuleRange(hMod, moduleBase, moduleSize)) {
        log::Line("camera_hook: GetModuleRange failed");
        return false;
    }
    telemetry::SetModuleBase(moduleBase);

    void* match = cameraunlock::memory::ScanPatternInRange(moduleBase, moduleSize, kCameraPattern);
    if (!match) {
        log::Line("camera_hook: pattern not found in Yakuza0.exe");
        return false;
    }

    const uintptr_t hookAddr = reinterpret_cast<uintptr_t>(match);

    // The hook contract (xmm4/5/6 register meaning, [rax+0xAC] FOV, rbx camera
    // object) only holds at the one intended site. If a game patch makes the
    // pattern ambiguous, hooking the first match would corrupt arbitrary code -
    // stay dormant instead.
    const uintptr_t secondStart = hookAddr + 1;
    void* second = cameraunlock::memory::ScanPatternInRange(
        secondStart, moduleSize - (secondStart - moduleBase), kCameraPattern);
    if (second) {
        log::Line("camera_hook: pattern is ambiguous (also matches RVA 0x%llX); refusing to hook",
                  (unsigned long long)(reinterpret_cast<uintptr_t>(second) - moduleBase));
        return false;
    }

    log::Line("camera_hook: pattern matched at %p (RVA 0x%llX)",
              match, (unsigned long long)(hookAddr - moduleBase));

    cul_camera_buffer = &g_cameraState;
    cul_camera_resume = reinterpret_cast<void*>(hookAddr + kPatchSize);

    // The trampoline lives in our DLL, potentially > 2 GB from the game module.
    // Allocate a near-page thunk that does an absolute jmp to it.
    g_nearThunk = BuildNearThunk(hookAddr, reinterpret_cast<void*>(&cul_camera_trampoline));
    if (!g_nearThunk) {
        log::Line("camera_hook: failed to allocate near-page thunk");
        return false;
    }

    int64_t rel64 = reinterpret_cast<int64_t>(g_nearThunk) - static_cast<int64_t>(hookAddr + kPatchSize);
    if (rel64 < INT32_MIN || rel64 > INT32_MAX) {
        log::Line("camera_hook: near thunk out of range (rel=%lld)", (long long)rel64);
        return false;
    }

    uint8_t patch[kPatchSize];
    patch[0] = 0xE9;
    int32_t rel = static_cast<int32_t>(rel64);
    std::memcpy(patch + 1, &rel, sizeof(rel));

    const PatchResult r = PatchCodeWithRetries(hookAddr, patch, sizeof(patch));
    if (r != PatchResult::Ok) {
        log::Line("camera_hook: %s; refusing to hook",
                  r == PatchResult::ThreadInPatchWindow
                      ? "a game thread is executing the hook site"
                      : "code patch failed (VirtualProtect/thread snapshot)");
        return false;
    }

    log::Line("camera_hook: installed (thunk=%p trampoline=%p)",
              g_nearThunk, reinterpret_cast<void*>(&cul_camera_trampoline));
    return true;
}

}  // namespace yakuza0

// ---------------------------------------------------------------------------
// Symbols visible to camera_hook.asm.
// ---------------------------------------------------------------------------
extern "C" void cul_camera_inject(yakuza0::CameraState* state) {
    yakuza0::ApplyTrackingToCamera(state);
}
