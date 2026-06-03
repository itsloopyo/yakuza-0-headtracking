#include "mod_config.h"

#include "logging.h"

#include <cameraunlock/config/ini_reader.h>

#include <windows.h>

namespace yakuza0 {

namespace {

constexpr int kMinVirtualKey = 0x01;
constexpr int kMaxVirtualKey = 0xFE;

HMODULE ThisModule() {
    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&ThisModule),
        &self);
    return self;
}

std::wstring ModuleSiblingPathW(const wchar_t* fileName) {
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(ThisModule(), modulePath, MAX_PATH);
    std::wstring path(modulePath);
    return path.substr(0, path.find_last_of(L"\\/") + 1) + fileName;
}

std::string ModuleSiblingPathA(const char* fileName) {
    char modulePath[MAX_PATH] = {};
    GetModuleFileNameA(ThisModule(), modulePath, MAX_PATH);
    std::string path(modulePath);
    return path.substr(0, path.find_last_of("\\/") + 1) + fileName;
}

void WriteDefaultIni(cameraunlock::IniWriter& writer) {
    writer.WriteSection("General");
    writer.WriteComment("Yaw mode: true = horizon-locked yaw (default), false = camera-local");
    writer.WriteString("WorldSpaceYaw", "true");
    writer.WriteBlankLine();
    writer.WriteSection("Hotkeys");
    writer.WriteComment("Page Down - toggle world/local yaw");
    writer.WriteString("YawModeKey", "0x22");
}

}  // namespace

std::wstring LogFilePath() {
    return ModuleSiblingPathW(L"Yakuza0HeadTracking.log");
}

Config LoadConfig() {
    Config cfg;
    const std::string path = ModuleSiblingPathA("Yakuza0HeadTracking.ini");

    cameraunlock::IniReader ini;
    if (!ini.Open(path)) {
        cameraunlock::IniWriter writer;
        if (!writer.Open(path)) {
            log::Line("config: could not create %s; using defaults", path.c_str());
            return cfg;
        }
        WriteDefaultIni(writer);
        writer.Close();
        log::Line("config: wrote default %s", path.c_str());
        return cfg;
    }

    cfg.worldSpaceYaw = ini.ReadBool("General", "WorldSpaceYaw", true);
    cfg.yawModeKey    = ini.ReadHex("Hotkeys", "YawModeKey", VK_NEXT);
    // GetAsyncKeyState only accepts virtual key codes 0x01-0xFE; a corrupted or
    // hand-edited INI ("YawModeKey=0x99999") would otherwise feed an
    // out-of-range value to the poller, where it silently never fires. Fall
    // back to the default rather than leaving the toggle dead.
    if (cfg.yawModeKey < kMinVirtualKey || cfg.yawModeKey > kMaxVirtualKey) {
        log::Line("config: YawModeKey 0x%X out of range (0x01-0xFE); using default 0x%X",
                  cfg.yawModeKey, VK_NEXT);
        cfg.yawModeKey = VK_NEXT;
    }
    log::Line("config: loaded %s (WorldSpaceYaw=%d YawModeKey=0x%X)",
              path.c_str(), cfg.worldSpaceYaw ? 1 : 0, cfg.yawModeKey);
    return cfg;
}

}  // namespace yakuza0
