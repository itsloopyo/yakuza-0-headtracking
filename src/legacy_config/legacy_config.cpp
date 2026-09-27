// Frozen. See legacy_config.h.

#include "legacy_config.h"

#include "logging.h"

#include <cameraunlock/config/ini_reader.h>

namespace yakuza0::legacy {

namespace {

constexpr int kMinVirtualKey = 0x01;
constexpr int kMaxVirtualKey = 0xFE;
constexpr int kDefaultYawModeKey = 0x22;  // VK_NEXT (Page Down)

}  // namespace

bool Load(const std::string& ini_path, Config& out) {
    cameraunlock::IniReader ini;
    if (!ini.Open(ini_path)) return false;

    out.world_space_yaw = ini.ReadBool("General", "WorldSpaceYaw", true);
    out.yaw_mode_key    = ini.ReadHex("Hotkeys", "YawModeKey", kDefaultYawModeKey);
    if (out.yaw_mode_key < kMinVirtualKey || out.yaw_mode_key > kMaxVirtualKey) {
        log::Line("config: YawModeKey 0x%X out of range (0x01-0xFE); using default 0x%X",
                  out.yaw_mode_key, kDefaultYawModeKey);
        out.yaw_mode_key = kDefaultYawModeKey;
    }
    log::Line("config: loaded %s (WorldSpaceYaw=%d YawModeKey=0x%X)",
              ini_path.c_str(), out.world_space_yaw ? 1 : 0, out.yaw_mode_key);
    return true;
}

std::vector<Key> ReadKeys() {
    return {
        {"General", "WorldSpaceYaw"},
        {"Hotkeys", "YawModeKey"},
    };
}

}  // namespace yakuza0::legacy
