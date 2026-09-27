// The dev pre-release's reader and startup code (commit 24bc0ea, the rolling
// dev pre-release, 0.0.0-nightly.20260820.24bc0ea). No v* release was
// published.
//
// src/mod_config.cpp, src/mod_config.h and src/logging.h beside this file are
// byte copies of 24bc0ea's, compiled here as they shipped, with their namespace
// renamed by the macro below so they can sit in one program beside this build's
// yakuza0. The build found its config beside the mod DLL through
// GetModuleFileNameA; the macro below points that one call at the folder a
// reading runs in, which is the only thing the test changes about it. The
// cameraunlock-core sources they compile hold the same code at 24bc0ea's pin
// (3465659) and at this repo's (CMakeLists.txt pins them by hash). What is
// transcribed is the startup code that consumed the settings, which cannot be
// compiled into a test because it hooks the game:
//
//   src/mod.cpp  lines 44, 47   g_modEnabled{true}, g_worldSpaceYaw{true}
//                lines 49-83    RegisterHotkeys, as data
//                lines 107-108  the yaw mode from the config
//                lines 117-133  SetMode(RotationAndPosition), the session's
//                               default smoothing, and PositionSettings{} with
//                               invert_z and its z bounds swapped (limit_z 0.10,
//                               limit_z_back 0.40), which is a forward lean of
//                               0.40 and a backward lean of 0.10
//                line 136       g_receiver.Start(), port 4242

#include <windows.h>

#include <string>

namespace yakuza0_oracle {
std::string g_dir;
}  // namespace yakuza0_oracle

namespace {

DWORD OracleModuleFileNameA(HMODULE, LPSTR buffer, DWORD size) {
    const std::string path = yakuza0_oracle::g_dir + "\\Yakuza0HeadTracking.asi";
    if (path.size() + 1 > size) return 0;
    path.copy(buffer, path.size());
    buffer[path.size()] = '\0';
    return static_cast<DWORD>(path.size());
}

}  // namespace

#define GetModuleFileNameA OracleModuleFileNameA
#define yakuza0 yakuza0_dev
#include "src/mod_config.cpp"
#undef yakuza0
#undef GetModuleFileNameA

#include "oracle_reader.h"

namespace yakuza0_oracle {

namespace {

constexpr unsigned kNav = 0;
constexpr unsigned kChord = 3;

}  // namespace

Published Read(const std::string& dir) {
    g_dir = dir;
    const yakuza0_dev::Config cfg = yakuza0_dev::LoadConfig();

    Published p;
    p.udp_port = 4242;
    p.tracking_enabled = true;
    p.world_space_yaw = cfg.worldSpaceYaw;
    p.tracking_mode = 0;
    p.local_smoothing = 0.0f;
    p.remote_smoothing = 0.15f;
    p.limit_x = 0.30f;
    p.limit_y = 0.20f;
    p.limit_y_down = 0.20f;
    p.limit_forward = 0.40f;
    p.limit_back = 0.10f;

    p.hotkeys.push_back({kToggle, VK_END, kNav});
    p.hotkeys.push_back({kCycleMode, VK_PRIOR, kNav});
    p.hotkeys.push_back({kYawMode, cfg.yawModeKey, kNav});
    p.hotkeys.push_back({kToggle, 'Y', kChord});
    p.hotkeys.push_back({kCycleMode, 'G', kChord});
    p.hotkeys.push_back({kYawMode, 'H', kChord});
    return p;
}

}  // namespace yakuza0_oracle
