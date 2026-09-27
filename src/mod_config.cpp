#include "mod_config.h"

#include "legacy_config/legacy_config.h"
#include "logging.h"

#include <cameraunlock/input/key_bindings.h>

#include <windows.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace yakuza0 {

namespace {

HMODULE ThisModule() {
    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&ThisModule),
        &self);
    return self;
}

}  // namespace

std::wstring ModuleDir() {
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(ThisModule(), modulePath, MAX_PATH) == 0) {
        throw std::runtime_error("GetModuleFileNameW failed for the mod DLL");
    }
    const std::wstring path(modulePath);
    return path.substr(0, path.find_last_of(L"\\/"));
}

std::wstring LogFilePath() {
    return ModuleDir() + L"\\Yakuza0HeadTracking.log";
}

std::wstring PrevLogFilePath() {
    return ModuleDir() + L"\\Yakuza0HeadTracking.prev.log";
}

}  // namespace yakuza0

namespace yakuza0::config {

namespace {

namespace cfg = ::cameraunlock::config;
using cfg::schema::Concept;
using ::cameraunlock::input::FormatKeyBindings;
using ::cameraunlock::input::KeyModifiers;

constexpr const wchar_t* kIniName = L"CameraUnlock.ini";
constexpr const wchar_t* kLegacyIniName = L"Yakuza0HeadTracking.ini";

// data/games.json's display_name for yakuza-0.
constexpr const char* kDisplayName = "Yakuza 0";

constexpr KeyModifiers kChord = KeyModifiers::kCtrl | KeyModifiers::kShift;

// The keys the dev build bound in code rather than in the file.
constexpr int kVkEnd = 0x23;
constexpr int kVkPageUp = 0x21;
constexpr int kVkY = 0x59;
constexpr int kVkG = 0x47;
constexpr int kVkH = 0x48;

std::unique_ptr<cfg::ConfigOwner<Config>> g_owner;

void Save(const char* rows, const std::function<void(Config&)>& change) {
    const cfg::ConfigSaveResult result = g_owner->Save(change);
    if (result.status != cfg::ConfigSaveStatus::Saved) {
        log::Line("config: %s %s: %s", rows, cfg::ConfigSaveStatusName(result.status), result.reason.c_str());
    }
    for (const std::string& line : result.log) log::Line("config: %s", line.c_str());
}

// A list of the non-empty items, joined as a key list.
std::string JoinBindings(const std::string& first, const std::string& second) {
    if (first.empty()) return second;
    if (second.empty()) return first;
    return first + ", " + second;
}

cfg::ImportResult RunImport(const cfg::LegacyInput& input, Config& out) {
    // The dev build opened Yakuza0HeadTracking.ini by its ANSI path, and the
    // frozen reader does the same. Where it finds no file, the dev build wrote
    // its default file and ran on its defaults.
    legacy::Config read;
    const bool present = legacy::Load(input.ansi_path, read);

    std::vector<cfg::DroppedValue> dropped;

    // The file held only these two. The reader keeps the yaw key inside
    // 0x01-0xFE, so N1 never applies; a bare Ctrl, Shift or Alt imports as
    // unbound (N3) and keeps the chord.
    out.world_space_yaw = read.world_space_yaw;
    out.yaw_mode_key = JoinBindings(
        cfg::LegacyVirtualKeyToBindings(read.yaw_mode_key, "Hotkeys", "YawModeKey", dropped),
        FormatKeyBindings({{kChord, kVkH}}));

    // End, Page Up and the Ctrl+Shift chords were bound in code. The dev build
    // started with tracking on, in rotation and position, listened on 4242 and
    // ran the session's default smoothing and lean limits, which are the
    // table's defaults.
    out.toggle_key = FormatKeyBindings({{KeyModifiers::kNone, kVkEnd}, {kChord, kVkY}});
    out.cycle_tracking_mode_key = FormatKeyBindings({{KeyModifiers::kNone, kVkPageUp}, {kChord, kVkG}});

    // A setting still at what the dev build shipped is no player's choice, so it
    // follows Defaults.ini. The frozen struct's defaults are what it shipped: its
    // first-run file wrote the same values.
    const legacy::Config shipped;
    cfg::LegacyFollowsDefaultsIni follows;
    follows.NotInLegacy(Concept::UdpPort);
    follows.NotInLegacy(Concept::EnableOnStartup);
    follows.Setting(Concept::WorldSpaceYaw, read.world_space_yaw, shipped.world_space_yaw);
    follows.TrackingMode(true);
    follows.NotInLegacy(Concept::LocalSmoothing);
    follows.NotInLegacy(Concept::RemoteSmoothing);
    follows.NotInLegacy(Concept::PositionLimitX);
    follows.NotInLegacy(Concept::PositionLimitY);
    follows.NotInLegacy(Concept::PositionLimitYDown);
    follows.NotInLegacy(Concept::PositionLimitZ);
    follows.NotInLegacy(Concept::PositionLimitZBack);
    follows.NotInLegacy(Concept::ToggleKey);
    follows.NotInLegacy(Concept::CycleTrackingModeKey);
    follows.Setting(Concept::YawModeKey, read.yaw_mode_key, shipped.yaw_mode_key);

    return present ? cfg::ImportResult::Imported(std::move(dropped), {}, follows.Concepts())
                   : cfg::ImportResult::Absent(std::move(dropped), {}, follows.Concepts());
}

}  // namespace

cfg::ConfigTable<Config> Table() {
    cfg::ConfigTable<Config> table;
    table.Concept<Concept::UdpPort>(&Config::udp_port)
        .Concept<Concept::EnableOnStartup>(&Config::enable_on_startup)
        .Concept<Concept::WorldSpaceYaw>(&Config::world_space_yaw)
        .Writable()
        .Concept<Concept::RotationEnabled>(&Config::rotation_enabled)
        .Writable()
        .Concept<Concept::LocalSmoothing>(&Config::local_smoothing)
        .Concept<Concept::RemoteSmoothing>(&Config::remote_smoothing)
        .Concept<Concept::PositionEnabled>(&Config::position_enabled)
        .Writable()
        .Concept<Concept::PositionLimitX>(&Config::position_limit_x)
        .Concept<Concept::PositionLimitY>(&Config::position_limit_y)
        .Concept<Concept::PositionLimitYDown>(&Config::position_limit_y_down)
        .Concept<Concept::PositionLimitZ>(&Config::position_limit_z)
        .Concept<Concept::PositionLimitZBack>(&Config::position_limit_z_back)
        .Concept<Concept::ToggleKey>(&Config::toggle_key)
        .Concept<Concept::CycleTrackingModeKey>(&Config::cycle_tracking_mode_key)
        .Concept<Concept::YawModeKey>(&Config::yaw_mode_key);
    return table;
}

cfg::RenderHeader Header() {
    cfg::RenderHeader header;
    header.display_name = kDisplayName;
    return header;
}

cfg::LegacyImport<Config> Import() {
    cfg::LegacyImport<Config> import;
    import.run = &RunImport;
    for (const legacy::Key& key : legacy::ReadKeys()) import.keys.push_back({key.section, key.key});
    return import;
}

cfg::ConfigOwnerOptions<Config> OwnerOptions(const std::wstring& dir, cfg::DefaultsFile defaults) {
    cfg::ConfigOwnerOptions<Config> options;
    options.path = dir + L"\\" + kIniName;
    options.table = Table();
    options.import = Import();
    options.legacy_path = dir + L"\\" + kLegacyIniName;
    options.header = Header();
    options.defaults = std::move(defaults);
    return options;
}

Config Load(const std::wstring& dir, cfg::DefaultsFile defaults) {
    g_owner = std::make_unique<cfg::ConfigOwner<Config>>(OwnerOptions(dir, std::move(defaults)));
    const cfg::ConfigLoadResult<Config> result = g_owner->Load();
    for (const std::string& line : result.log) log::Line("config: %s", line.c_str());
    if (!result.reason.empty()) log::Line("config: %s", result.reason.c_str());
    log::Line("config: %s", cfg::ConfigLoadStatusName(result.status));
    return result.config;
}

cameraunlock::TrackingMode StartupTrackingMode(const Config& config) {
    const auto mode = cameraunlock::DecodeTrackingMode(config.rotation_enabled, config.position_enabled);
    if (!mode) throw std::logic_error("RotationEnabled and PositionEnabled are both false, which the table never gives");
    return *mode;
}

void SaveWorldSpaceYaw(bool world_space_yaw) {
    Save("[General] WorldSpaceYaw", [world_space_yaw](Config& c) { c.world_space_yaw = world_space_yaw; });
}

void SaveTrackingMode(cameraunlock::TrackingMode mode) {
    const cameraunlock::TrackingModeChannels channels = cameraunlock::EncodeTrackingMode(mode);
    Save("[General] RotationEnabled and [Position] PositionEnabled", [channels](Config& c) {
        c.rotation_enabled = channels.rotation_enabled;
        c.position_enabled = channels.position_enabled;
    });
}

}  // namespace yakuza0::config
