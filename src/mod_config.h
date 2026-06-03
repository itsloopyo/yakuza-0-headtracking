#pragma once

#include <string>

namespace yakuza0 {

struct Config {
    bool worldSpaceYaw = true;
    int  yawModeKey    = 0x22;  // VK_NEXT (Page Down)
};

// Loads Yakuza0HeadTracking.ini from next to the mod DLL. A missing file or
// missing entries fall back to defaults; a default INI is written on first
// run so users have something to edit.
Config LoadConfig();

// Absolute path of the mod's log file, next to the mod DLL.
std::wstring LogFilePath();

}  // namespace yakuza0
