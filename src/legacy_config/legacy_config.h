#pragma once

#include <string>
#include <vector>

// The pre-canonical Yakuza0HeadTracking.ini reader, frozen. It reads a file the
// way the last build before the canonical config format did, so a player's old
// file is carried over as that build read it. Never edit anything in this
// folder: CMakeLists.txt pins every file here by hash.
//
// Frozen from src/mod_config.cpp and src/mod_config.h at fb48e64 (LoadConfig),
// which hold the dev pre-release's (24bc0ea) code byte for byte, with three
// changes: it fills this frozen copy of that commit's settings and their
// defaults instead of the mod's Config, it writes nothing (the first-run
// default file stays with the mod), and it lives in namespace
// yakuza0::legacy. IniReader is core's, which core keeps frozen.
namespace yakuza0::legacy {

struct Config {
    // [General]
    bool world_space_yaw = true;

    // [Hotkeys]. A virtual-key code, read with IniReader::ReadHex; one outside
    // 0x01-0xFE is replaced by 0x22.
    int yaw_mode_key = 0x22;  // VK_NEXT (Page Down)
};

// Reads `ini_path` into `out`; keys the file lacks keep their defaults. Returns
// whether the file was there to read (IniReader::Open).
bool Load(const std::string& ini_path, Config& out);

struct Key {
    const char* section;
    const char* key;
};

// Every key Load takes a value from.
std::vector<Key> ReadKeys();

}  // namespace yakuza0::legacy
