// The differential test for the conversion from Yakuza0HeadTracking.ini to the
// canonical config format.
//
//   Oracle     the dev pre-release's reader and startup code, the newest and
//              only published build (oracle/oracle_reader.cpp)
//   Import     the frozen reader in src/legacy_config/, through the startup
//              code the commit that froze it ran it through
//   Migration  the config owner's Load in a folder holding only the input as
//              Yakuza0HeadTracking.ini, which imports it into a new
//              CameraUnlock.ini, then the canonical reader and table on it,
//              through this build's startup code
//
// Comparison 1, oracle against import, is what a player sees change that the
// conversion did not cause: commits since the dev build that change how the file
// is read. There are none. The reader's source is the same at 24bc0ea and at the
// commit that froze it, and every core source the two compile holds the same
// code at both pins, so every field must agree bit for bit.
//
// Comparison 2, import against migration, is the proof for the conversion; see
// the section of that name below for what it allows.
//
// Inputs: the file the dev build writes on its first run (no release shipped or
// seeded a Yakuza0HeadTracking.ini), no file, an empty file, core's mutation
// corpus over the first-run file, and that file with YawModeKey set to every
// code from 0x00 to 0xFF.

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "legacy_config/legacy_config.h"
#include "mod_config.h"
#include "oracle/oracle_reader.h"

#include "cameraunlock/config/canonical_ini.h"
#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_bindings.h"

namespace {

namespace fs = std::filesystem;
namespace cfg = cameraunlock::config;
namespace legacy = yakuza0::legacy;
namespace testing = cameraunlock::config::testing;

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const std::string& what) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL: %s\n", what.c_str());
}

std::string ReadFileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFileBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

// ---- Scratch folders ---------------------------------------------------------
//
// One folder per reading: GetPrivateProfileString, which every reader here sits
// on, is free to cache the file it last read. `game` stands for the folder the
// mod DLL sits in (media\), and Defaults.ini sits in `global` beside it. Every
// folder lives under one root for the run, removed once at the end.

void RemoveTree(const fs::path& root) {
    if (!fs::exists(root)) return;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
    }
    fs::remove_all(root);
}

const fs::path& ScratchRoot() {
    static const fs::path root = [] {
        wchar_t temp[MAX_PATH + 1] = {};
        if (GetTempPathW(MAX_PATH + 1, temp) == 0) throw std::runtime_error("GetTempPathW failed");
        fs::path r = fs::path(temp) / ("yakuza0_ht_diff_" + std::to_string(GetCurrentProcessId()));
        RemoveTree(r);
        return r;
    }();
    return root;
}

class Scratch {
public:
    Scratch() {
        static unsigned s_next = 0;
        root_ = ScratchRoot() / std::to_string(s_next++);
        fs::create_directories(root_ / "game");
    }

    fs::path game() const { return root_ / "game"; }
    fs::path legacy() const { return game() / "Yakuza0HeadTracking.ini"; }
    fs::path canonical() const { return game() / "CameraUnlock.ini"; }
    fs::path defaults() const { return root_ / "global" / "Defaults.ini"; }

    void WriteLegacy(const std::string& bytes) const { WriteFileBytes(legacy(), bytes); }
    void WriteDefaults(const std::string& bytes) const {
        fs::create_directories(defaults().parent_path());
        WriteFileBytes(defaults(), bytes);
    }

private:
    fs::path root_;
};

// ---- What a reading does -------------------------------------------------------
//
// A Record names everything the running mod acts on after reading the file:
// `field.*` the settings, `start.*` the state the session starts in, `hotkey.*`
// the bindings that can fire, each as `modifiers:code` (Ctrl 1, Shift 2, as
// cameraunlock::input::KeyModifiers numbers them) in ascending order. Floats are
// their bits.

using Record = std::map<std::string, std::string>;

std::string Bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", static_cast<unsigned>(bits));
    return text;
}

std::string Flag(bool value) { return value ? "1" : "0"; }

const char* const kActionNames[] = {"Toggle", "CycleTrackingMode", "YawMode"};

// The bindings a set of HotkeyPoller registrations can fire. The poller skips
// code 0, and GetAsyncKeyState reports no code above 0xFF or below 0 down.
void AddHotkeys(Record& r, const std::vector<yakuza0_oracle::Registration>& registrations) {
    std::map<int, std::vector<std::pair<unsigned, int>>> byAction;
    for (int action = 0; action < 3; ++action) byAction[action];
    for (const auto& [action, vk, modifiers] : registrations) {
        if (vk < 0x01 || vk > 0xFF) continue;
        byAction[action].push_back({modifiers, vk});
    }
    for (auto& [action, items] : byAction) {
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end()), items.end());
        std::string text;
        for (const auto& [modifiers, vk] : items) {
            char item[32];
            std::snprintf(item, sizeof(item), "%s%u:0x%02X", text.empty() ? "" : " ", modifiers, static_cast<unsigned>(vk));
            text += item;
        }
        r[std::string("hotkey.") + kActionNames[action]] = text;
    }
}

const char* ModeName(int mode) {
    switch (mode) {
        case 0: return "RotationAndPosition";
        case 1: return "RotationOnly";
        case 2: return "PositionOnly";
        default: return "none";
    }
}

Record ObserveOracle(const yakuza0_oracle::Published& g) {
    Record r;
    r["field.udp_port"] = std::to_string(g.udp_port);
    r["field.local_smoothing"] = Bits(g.local_smoothing);
    r["field.remote_smoothing"] = Bits(g.remote_smoothing);
    r["field.pos.limit_x"] = Bits(g.limit_x);
    r["field.pos.limit_y"] = Bits(g.limit_y);
    r["field.pos.limit_y_down"] = Bits(g.limit_y_down);
    r["field.pos.limit_forward"] = Bits(g.limit_forward);
    r["field.pos.limit_back"] = Bits(g.limit_back);
    r["start.enabled"] = Flag(g.tracking_enabled);
    r["start.mode"] = ModeName(g.tracking_mode);
    r["start.world_space_yaw"] = Flag(g.world_space_yaw);
    AddHotkeys(r, g.hotkeys);
    return r;
}

// The frozen reader's settings through the startup code of the commit that
// froze it, which is the dev build's: src/mod.cpp is unchanged since 24bc0ea.
Record ObserveImport(const legacy::Config& c) {
    yakuza0_oracle::Published g;
    g.udp_port = 4242;
    g.tracking_enabled = true;
    g.world_space_yaw = c.world_space_yaw;
    g.tracking_mode = 0;
    g.local_smoothing = 0.0f;
    g.remote_smoothing = 0.15f;
    g.limit_x = 0.30f;
    g.limit_y = 0.20f;
    g.limit_y_down = 0.20f;
    g.limit_forward = 0.40f;
    g.limit_back = 0.10f;
    g.hotkeys = {{yakuza0_oracle::kToggle, 0x23, 0},
                 {yakuza0_oracle::kCycleMode, 0x21, 0},
                 {yakuza0_oracle::kYawMode, c.yaw_mode_key, 0},
                 {yakuza0_oracle::kToggle, 0x59, 3},
                 {yakuza0_oracle::kCycleMode, 0x47, 3},
                 {yakuza0_oracle::kYawMode, 0x48, 3}};
    return ObserveOracle(g);
}

std::vector<std::string> Differences(const Record& a, const Record& b) {
    std::vector<std::string> out;
    for (const auto& [name, value] : a) {
        const auto it = b.find(name);
        if (it == b.end()) {
            out.push_back(name + " only on the left");
        } else if (it->second != value) {
            out.push_back(name + ": " + value + " / " + it->second);
        }
    }
    for (const auto& [name, value] : b) {
        if (a.find(name) == a.end()) out.push_back(name + " only on the right");
    }
    return out;
}

// ---- Inputs --------------------------------------------------------------------

fs::path DataPath(const char* name) {
    return fs::path(YAKUZA0_SOURCE_DIR) / "tests" / "config_differential" / "data" / name;
}

// What the dev build's LoadConfig wrote on the first run: the file every player
// who ran a published build holds, edited or not.
std::string FirstRun() { return ReadFileBytes(DataPath("first-run-dev.ini")); }

const char* const kFirstRunName = "dev first-run file";

// Every key the frozen reader reads, and how the corpus varies each one. The
// reader replaces a yaw key outside 0x01-0xFE with 0x22.
std::vector<testing::MutationKey> CorpusKeys() {
    return {
        {"General", "WorldSpaceYaw", "false", {}},
        {"Hotkeys", "YawModeKey", "0x2E", {"0x100"}, true},
    };
}

std::vector<cfg::LegacyKey> CorpusReads() {
    std::vector<cfg::LegacyKey> keys;
    for (const legacy::Key& key : legacy::ReadKeys()) keys.push_back({key.section, key.key});
    return keys;
}

struct Input {
    std::string name;
    bool present;
    std::string bytes;
};

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    if (at == std::string::npos) throw std::logic_error("'" + from + "' is not in the text");
    return text.replace(at, from.size(), to);
}

std::vector<Input> Inputs() {
    std::vector<Input> inputs = {
        {kFirstRunName, true, FirstRun()},
        {"no file", false, {}},
        {"empty file", true, {}},
    };
    for (testing::IniMutation& m : testing::GenerateIniMutations(FirstRun(), CorpusReads(), CorpusKeys())) {
        inputs.push_back({"corpus: " + m.name, true, std::move(m.bytes)});
    }
    for (int code = 0x00; code <= 0xFF; ++code) {
        char text[32];
        std::snprintf(text, sizeof(text), "YawModeKey=0x%02X", static_cast<unsigned>(code));
        inputs.push_back({text, true, Replace(FirstRun(), "YawModeKey=0x22", text)});
    }
    return inputs;
}

// ---- Comparison 1 ----------------------------------------------------------------

void Compare(const std::vector<Input>& inputs) {
    int compared = 0;
    for (const Input& input : inputs) {
        const std::string& name = input.name;
        Scratch s;
        if (input.present) s.WriteLegacy(input.bytes);
        const Record oracle = ObserveOracle(yakuza0_oracle::Read(s.game().string()));

        // The dev build writes its default file where none exists, and nothing else.
        if (!input.present) {
            Check(ReadFileBytes(s.legacy()) == FirstRun(), name + ": the dev build's first run writes the committed first-run file");
        } else {
            Check(ReadFileBytes(s.legacy()) == input.bytes, name + ": the dev build leaves an existing file as it was");
        }

        // The frozen reader, on its own copy of the input, so it finds no
        // file where the player had none.
        Scratch t;
        if (input.present) t.WriteLegacy(input.bytes);
        legacy::Config read;
        const bool present = legacy::Load(t.legacy().string(), read);
        Check(present == input.present, name + ": the frozen reader finds the file exactly when it is there");
        Check(input.present || !fs::exists(t.legacy()), name + ": the frozen reader writes no file");
        const Record imported = ObserveImport(read);

        const std::vector<std::string> diff = Differences(oracle, imported);
        for (const std::string& d : diff) std::printf("  comparison 1, %s: %s\n", name.c_str(), d.c_str());
        Check(diff.empty(), name + ": comparison 1, the oracle and the import agree");
        ++compared;
    }
    std::printf("comparison 1: %d inputs\n", compared);
}

// ---- Comparison 2 ------------------------------------------------------------------
//
// The migration: the config owner's Load in a folder holding only the input as
// Yakuza0Yakuza0HeadTracking.ini, which imports it through config::Import into a new
// CameraUnlock.ini, then the canonical reader and table on that file. It must
// start the mod exactly as the import did, apart from what core's
// data/config-format.json approves:
//
//   N3  a yaw key on Ctrl, Shift or Alt alone (0x10-0x12, 0xA0-0xA5) imports
//       as unbound and is logged; its Ctrl+Shift+H chord stays.
//
// The frozen reader keeps the yaw key inside 0x01-0xFE, so N1 never applies,
// and it reads no float, so N2 never applies. The dev build had no setting for
// the port, the startup state, the smoothing or the lean limits; its hard-coded
// values are the table's defaults, which the migration writes as `default`.
//
// Each input with a file migrates three times: over a Defaults.ini the owner
// creates with the built-in values, from a read-only Yakuza0Yakuza0HeadTracking.ini,
// and over a Defaults.ini that differs from the built-in value on every row.
// The first two give the settings the import read. A setting still at what the
// dev build shipped, or one it had no setting for, is no player's choice (owner
// rule of 2026-09-26), so it is written `default` and the third gives
// Defaults.ini's value for it; a setting the player changed keeps the imported
// value there too.

using Drop = std::tuple<cfg::DropRule, std::string, std::string>;

// The settings the running mod acts on from a canonical Config, through this
// build's startup code: mod.cpp InitThread, which hands the port to the
// receiver, the smoothing and the lean limits to the session, and the forward
// lean as PositionLimitZ now that z is flipped after the clamp, and
// RegisterHotkeys, which puts each list through ParseKeyBindings and
// RegisterKeyBindings.
Record ObserveCanonical(const yakuza0::Config& c) {
    yakuza0_oracle::Published g;
    g.udp_port = c.udp_port;
    g.tracking_enabled = c.enable_on_startup;
    g.world_space_yaw = c.world_space_yaw;
    g.tracking_mode = static_cast<int>(yakuza0::config::StartupTrackingMode(c));
    g.local_smoothing = c.local_smoothing;
    g.remote_smoothing = c.remote_smoothing;
    g.limit_x = c.position_limit_x;
    g.limit_y = c.position_limit_y;
    g.limit_y_down = c.position_limit_y_down;
    g.limit_forward = c.position_limit_z;
    g.limit_back = c.position_limit_z_back;
    const std::pair<int, const std::string*> lists[] = {
        {yakuza0_oracle::kToggle, &c.toggle_key},
        {yakuza0_oracle::kCycleMode, &c.cycle_tracking_mode_key},
        {yakuza0_oracle::kYawMode, &c.yaw_mode_key},
    };
    for (const auto& [action, list] : lists) {
        const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(*list);
        Check(parsed.ok(), "a migrated key list parses: " + *list);
        for (const cameraunlock::input::KeyBinding& b : parsed.bindings) {
            g.hotkeys.push_back({action, b.vk, static_cast<unsigned>(b.modifiers)});
        }
    }
    return ObserveOracle(g);
}

// The drops the approved changes call for, from what the frozen reader read, and
// the settings the session then runs on: comparison 2's whole allowance.
struct Allowed {
    std::vector<Drop> dropped;
    Record observed;
};

bool IsModifierKey(int vk) { return (vk >= 0x10 && vk <= 0x12) || (vk >= 0xA0 && vk <= 0xA5); }

Allowed ApplyApprovedChanges(const legacy::Config& read) {
    Allowed a;
    legacy::Config c = read;
    if (IsModifierKey(c.yaw_mode_key)) {
        a.dropped.push_back({cfg::DropRule::ModifierKey, "Hotkeys", "YawModeKey"});
        c.yaw_mode_key = 0;
    }
    a.observed = ObserveImport(c);
    return a;
}

std::vector<std::string> CanonicalDiagnostics(const std::string& bytes, yakuza0::Config& out) {
    std::vector<std::string> found;
    const cfg::CanonicalIni doc = cfg::ParseCanonicalIni(bytes);
    for (const cfg::CanonicalDiagnostic& d : doc.diagnostics) found.push_back("reader: " + cfg::DescribeCanonicalDiagnostic(d));
    const cfg::ConfigTable<yakuza0::Config> table = yakuza0::config::Table();
    out = table.defaults();
    for (const cfg::CanonicalDiagnostic& d : cfg::ApplyCanonical(doc, table, out).diagnostics) {
        found.push_back("table: " + cfg::DescribeCanonicalDiagnostic(d));
    }
    return found;
}

bool AsciiCrlf(const std::string& bytes) {
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (c > 0x7E) return false;
        if (c == '\r' && (i + 1 == bytes.size() || bytes[i + 1] != '\n')) return false;
        if (c == '\n' && (i == 0 || bytes[i - 1] != '\r')) return false;
        if (c < 0x20 && c != '\r' && c != '\n') return false;
    }
    return !bytes.empty() && bytes.back() == '\n';
}

// A file's bytes, last write time and attributes, which no load may change.
struct FileState {
    std::string bytes;
    unsigned long long written = 0;
    DWORD attributes = 0;
    bool operator==(const FileState& other) const {
        return bytes == other.bytes && written == other.written && attributes == other.attributes;
    }
};

std::optional<FileState> StateOf(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return std::nullopt;
        throw std::runtime_error("cannot read the attributes of " + path.string());
    }
    FileState state;
    state.bytes = ReadFileBytes(path);
    state.written = (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                    data.ftLastWriteTime.dwLowDateTime;
    state.attributes = data.dwFileAttributes;
    return state;
}

std::set<std::string> Names(const fs::path& folder) {
    std::set<std::string> names;
    for (const auto& entry : fs::directory_iterator(folder)) names.insert(entry.path().filename().string());
    return names;
}

bool LogSays(const std::vector<std::string>& log, const std::string& text) {
    for (const std::string& line : log) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

// A Defaults.ini holding a value other than the built-in one on every row the
// table binds, so a migration that wrote `default` where the imported value is
// not what `default` gives would read back differently over it.
const char* const kSkewedDefaults =
    "[CameraUnlock]\r\nConfigFormat=1\r\n\r\n"
    "[Network]\r\nUdpPort=5252\r\n\r\n"
    "[General]\r\nEnableOnStartup=false\r\nWorldSpaceYaw=false\r\nRotationEnabled=false\r\n\r\n"
    "[Smoothing]\r\nLocalSmoothing=0.5\r\nRemoteSmoothing=0.5\r\n\r\n"
    "[Position]\r\nPositionEnabled=true\r\nPositionLimitX=0.5\r\nPositionLimitY=0.5\r\nPositionLimitYDown=0.5\r\n"
    "PositionLimitZ=0.5\r\nPositionLimitZBack=0.5\r\n\r\n"
    "[Hotkeys]\r\nToggleKey=F8\r\nCycleTrackingModeKey=F9\r\nYawModeKey=F10\r\n";

// A row of the table and whether the player left it at what the dev build
// shipped, with what kSkewedDefaults gives it. The frozen struct's defaults are
// what the dev build shipped, and its first-run file writes the same values.
// Every row but WorldSpaceYaw and YawModeKey was no setting in the dev build.
struct FollowRow {
    const char* key;
    bool untouched;
    std::function<void(yakuza0::Config&)> skew;
};

std::vector<FollowRow> FollowRows(const legacy::Config& read) {
    const legacy::Config shipped;
    using C = yakuza0::Config;
    return {
        {"UdpPort", true, [](C& c) { c.udp_port = 5252; }},
        {"EnableOnStartup", true, [](C& c) { c.enable_on_startup = false; }},
        {"WorldSpaceYaw", read.world_space_yaw == shipped.world_space_yaw, [](C& c) { c.world_space_yaw = false; }},
        {"RotationEnabled", true, [](C& c) { c.rotation_enabled = false; }},
        {"PositionEnabled", true, [](C& c) { c.position_enabled = true; }},
        {"LocalSmoothing", true, [](C& c) { c.local_smoothing = 0.5f; }},
        {"RemoteSmoothing", true, [](C& c) { c.remote_smoothing = 0.5f; }},
        {"PositionLimitX", true, [](C& c) { c.position_limit_x = 0.5f; }},
        {"PositionLimitY", true, [](C& c) { c.position_limit_y = 0.5f; }},
        {"PositionLimitYDown", true, [](C& c) { c.position_limit_y_down = 0.5f; }},
        {"PositionLimitZ", true, [](C& c) { c.position_limit_z = 0.5f; }},
        {"PositionLimitZBack", true, [](C& c) { c.position_limit_z_back = 0.5f; }},
        {"ToggleKey", true, [](C& c) { c.toggle_key = "F8"; }},
        {"CycleTrackingModeKey", true, [](C& c) { c.cycle_tracking_mode_key = "F9"; }},
        {"YawModeKey", read.yaw_mode_key == shipped.yaw_mode_key, [](C& c) { c.yaw_mode_key = "F10"; }},
    };
}

// The value text of `key` in a canonical file, whose keys this table never
// repeats across sections; nullopt where the file has no such line.
std::optional<std::string> RowValue(const std::string& bytes, const std::string& key) {
    const std::string start = "\r\n" + key + "=";
    const std::size_t at = bytes.find(start);
    if (at == std::string::npos) return std::nullopt;
    const std::size_t from = at + start.size();
    return bytes.substr(from, bytes.find("\r\n", from) - from);
}

// The folder beside this executable the migrated files are written to, for
// lint-migrated.mjs, which CTest runs after this test.
fs::path MigratedFolder() {
    std::vector<wchar_t> exe(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        if (length == 0) throw std::runtime_error("cannot find this executable's path");
        if (length < exe.size()) return fs::path(std::wstring(exe.data(), length)).parent_path() / "migrated";
        exe.resize(exe.size() * 2);
    }
}

cfg::ConfigOwnerOptions<yakuza0::Config> OwnerOptions(const Scratch& s) {
    return yakuza0::config::OwnerOptions(s.game().wstring(), cfg::DefaultsFile::At(s.defaults().wstring()));
}

// Runs the owner's Load in `s`, whose game folder holds the input as
// Yakuza0HeadTracking.ini or nothing, checks what a load must do beyond comparison 2,
// and returns the settings the session runs on. A file it creates by migrating
// goes into `migrated_files`.
yakuza0::Config Migrate(const Input& input, const Scratch& s, const std::string& label,
                              std::set<std::string>& migrated_files) {
    const std::optional<FileState> legacy_before = StateOf(s.legacy());
    const std::optional<FileState> defaults_before = StateOf(s.defaults());

    const cfg::ConfigLoadResult<yakuza0::Config> loaded =
        cfg::ConfigOwner<yakuza0::Config>(OwnerOptions(s)).Load();
    const cfg::ConfigLoadStatus want = input.present ? cfg::ConfigLoadStatus::Migrated : cfg::ConfigLoadStatus::Created;
    if (loaded.status != want) {
        std::printf("  %s: %s, %s\n", label.c_str(), cfg::ConfigLoadStatusName(loaded.status), loaded.reason.c_str());
    }
    Check(loaded.status == want, label + ": the load is " + cfg::ConfigLoadStatusName(want));
    Check(StateOf(s.legacy()) == legacy_before, label + ": a load leaves Yakuza0HeadTracking.ini's bytes, write time and attributes");
    Check(!defaults_before || StateOf(s.defaults()) == defaults_before, label + ": a load leaves Defaults.ini as it was");
    if (loaded.status != want) return loaded.config;

    Check(Names(s.game()) == (input.present ? std::set<std::string>{"CameraUnlock.ini", "Yakuza0HeadTracking.ini"}
                                            : std::set<std::string>{"CameraUnlock.ini"}),
          label + ": the game folder holds Yakuza0HeadTracking.ini and CameraUnlock.ini and nothing else");

    const std::string migrated = ReadFileBytes(s.canonical());
    Check(cfg::HasCanonicalStamp(migrated), label + ": CameraUnlock.ini carries the stamp");
    Check(AsciiCrlf(migrated), label + ": CameraUnlock.ini is ASCII with CRLF line ends");
    yakuza0::Config reread;
    const std::vector<std::string> diagnostics = CanonicalDiagnostics(migrated, reread);
    for (const std::string& d : diagnostics) std::printf("  %s: CameraUnlock.ini, %s\n", label.c_str(), d.c_str());
    Check(diagnostics.empty(), label + ": CameraUnlock.ini reads with no diagnostic");
    if (input.present) migrated_files.insert(migrated);

    // The next start reads CameraUnlock.ini, imports nothing and writes nothing.
    const std::optional<FileState> created = StateOf(s.canonical());
    const cfg::ConfigLoadResult<yakuza0::Config> again =
        cfg::ConfigOwner<yakuza0::Config>(OwnerOptions(s)).Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical, label + ": the next start reads CameraUnlock.ini");
    Check(Differences(ObserveCanonical(again.config), ObserveCanonical(loaded.config)).empty(),
          label + ": the next start runs on the same settings");
    Check(StateOf(s.canonical()) == created && StateOf(s.legacy()) == legacy_before,
          label + ": the next start changes neither file");
    Check(!input.present || LogSays(again.log, "is left as it was and is not read"),
          label + ": the next start logs that Yakuza0HeadTracking.ini is not read");
    return loaded.config;
}

void ImportAgainstMigration(const std::vector<Input>& inputs) {
    const std::string committed = ReadFileBytes(fs::path(YAKUZA0_SOURCE_DIR) / "CameraUnlock.ini");
    const cfg::ConfigTable<yakuza0::Config> table = yakuza0::config::Table();
    std::set<std::string> migrated_files;
    int compared = 0;
    int dropping = 0;
    std::set<int> modifier_codes;
    std::map<std::string, std::pair<int, int>> follow_seen;  // untouched, changed
    for (const Input& input : inputs) {
        const std::string& name = input.name;

        // The import, run as the owner runs it but on a read-only copy: it reads
        // what the frozen reader reads, drops what the approved changes drop, and
        // writes nothing.
        cfg::ImportResult imported;
        legacy::Config read;
        {
            Scratch ro;
            if (input.present) {
                ro.WriteLegacy(input.bytes);
                SetFileAttributesW(ro.legacy().c_str(), FILE_ATTRIBUTE_READONLY);
            }
            const std::optional<FileState> before = StateOf(ro.legacy());
            yakuza0::Config unused = table.defaults();
            imported = yakuza0::config::Import().run({ro.legacy().wstring(), ro.legacy().string(), false}, unused);
            const std::set<std::string> left = input.present ? std::set<std::string>{"Yakuza0HeadTracking.ini"}
                                                             : std::set<std::string>{};
            Check(StateOf(ro.legacy()) == before && Names(ro.game()) == left,
                  name + ": the import leaves a read-only folder as it was");
            legacy::Load(ro.legacy().string(), read);
        }
        Check(imported.status == (input.present ? cfg::ImportStatus::Imported : cfg::ImportStatus::Absent),
              name + ": the import reads every input, as the published build did");

        const Allowed allowed = ApplyApprovedChanges(read);
        const std::vector<FollowRow> follow = FollowRows(read);
        for (const FollowRow& row : follow) {
            auto& seen = follow_seen[row.key];
            ++(row.untouched ? seen.first : seen.second);
        }
        if (!allowed.dropped.empty()) ++dropping;
        if (IsModifierKey(read.yaw_mode_key)) modifier_codes.insert(read.yaw_mode_key);
        std::vector<Drop> dropped;
        for (const cfg::DroppedValue& d : imported.dropped) dropped.push_back({d.rule, d.section, d.key});
        std::sort(dropped.begin(), dropped.end());
        Check(dropped == allowed.dropped, name + ": the import drops exactly what the approved changes drop");
        Check(imported.pose_shaping.empty(), name + ": the dev build had no pose-shaping setting");

        // Over a Defaults.ini the owner creates with the built-in values.
        yakuza0::Config migrated_over_built_in;
        {
            Scratch s;
            if (input.present) s.WriteLegacy(input.bytes);
            const yakuza0::Config migrated = Migrate(input, s, name, migrated_files);
            migrated_over_built_in = migrated;
            const std::vector<std::string> diff = Differences(allowed.observed, ObserveCanonical(migrated));
            for (const std::string& d : diff) std::printf("  comparison 2, %s: %s\n", name.c_str(), d.c_str());
            Check(diff.empty(), name + ": comparison 2, the migration runs as the import read, less the approved changes");

            // Over the built-in values the table's own defaults stand for Defaults.ini.
            yakuza0::Config reread;
            CanonicalDiagnostics(ReadFileBytes(s.canonical()), reread);
            Check(Differences(ObserveCanonical(reread), ObserveCanonical(migrated)).empty(),
                  name + ": CameraUnlock.ini reads back as the settings the session runs on");

            // A row the player left at what the dev build shipped is written
            // default. One they changed holds its value: every shipped value is
            // the schema's, so a changed one is never what default gives here.
            const std::string written = ReadFileBytes(s.canonical());
            for (const FollowRow& row : follow) {
                const std::optional<std::string> value = RowValue(written, row.key);
                Check(value.has_value(), name + ": CameraUnlock.ini has a " + row.key + " row");
                if (!value) continue;
                Check((*value == "default") == row.untouched,
                      name + ": " + row.key + "=" + *value +
                          (row.untouched ? " follows Defaults.ini, as the player never changed it"
                                         : " is the player's own value"));
            }

            // Fresh equals upgrade: the published build's first-run file, and no
            // file at all, both end as the committed file.
            if (name == kFirstRunName || name == "no file") {
                Check(ReadFileBytes(s.canonical()) == committed, name + ": gives the committed file byte for byte");
            }
        }

        if (!input.present) {
            ++compared;
            continue;
        }

        // From a read-only Yakuza0HeadTracking.ini, which keeps its attribute.
        {
            Scratch ro;
            ro.WriteLegacy(input.bytes);
            SetFileAttributesW(ro.legacy().c_str(), FILE_ATTRIBUTE_READONLY);
            const yakuza0::Config c = Migrate(input, ro, name + " (read-only)", migrated_files);
            Check(Differences(allowed.observed, ObserveCanonical(c)).empty(),
                  name + ": a read-only Yakuza0HeadTracking.ini imports as a writable one does");
            Check((GetFileAttributesW(ro.legacy().c_str()) & FILE_ATTRIBUTE_READONLY) != 0,
                  name + ": Yakuza0HeadTracking.ini keeps its read-only attribute");
        }

        // Over a Defaults.ini that differs everywhere.
        {
            Scratch skewed;
            skewed.WriteLegacy(input.bytes);
            skewed.WriteDefaults(kSkewedDefaults);
            const yakuza0::Config c = Migrate(input, skewed, name + " (skewed Defaults.ini)", migrated_files);
            yakuza0::Config expected = migrated_over_built_in;
            for (const FollowRow& row : follow) {
                if (row.untouched) row.skew(expected);
            }
            const std::vector<std::string> diff = Differences(ObserveCanonical(expected), ObserveCanonical(c));
            for (const std::string& d : diff) std::printf("  comparison 2, %s (skewed Defaults.ini): %s\n", name.c_str(), d.c_str());
            Check(diff.empty(), name + ": comparison 2 over a Defaults.ini that differs everywhere, where each "
                                "setting the player never changed takes Defaults.ini's value");
        }
        ++compared;
    }
    std::printf("comparison 2: %d inputs, %d with a yaw key N3 unbinds\n", compared, dropping);
    Check(modifier_codes.size() == 9, "the inputs reach N3 on each of the nine Ctrl, Shift and Alt codes");
    for (const auto& [key, seen] : follow_seen) {
        const bool in_legacy = key == "WorldSpaceYaw" || key == "YawModeKey";
        Check(seen.first > 0 && (!in_legacy || seen.second > 0),
              "the inputs leave " + key + " at what the dev build shipped" + (in_legacy ? " and change it" : ""));
    }

    // Core's canonical config lint runs over these next (lint-migrated.mjs).
    const fs::path lint = MigratedFolder();
    fs::remove_all(lint);
    fs::create_directories(lint);
    std::size_t n = 0;
    for (const std::string& file : migrated_files) {
        WriteFileBytes(lint / (std::to_string(n++) + ".ini"), file);
    }
    std::printf("%zu distinct migrated files written to %s\n", migrated_files.size(), lint.string().c_str());
}

}  // namespace

int main() {
    // Unbuffered, so the lines before an exception reach the log.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        const std::vector<Input> inputs = Inputs();
        Compare(inputs);
        ImportAgainstMigration(inputs);
        RemoveTree(ScratchRoot());
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
