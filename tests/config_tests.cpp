// CameraUnlock.ini in the canonical config format.
//
// The committed CameraUnlock.ini is the table's fresh render, which is also what
// the owner creates beside the mod DLL at first launch: `default` on every row,
// so each follows Defaults.ini. A toggle's save changes the lines of its rows
// and no other byte. An older Yakuza0HeadTracking.ini is imported once into a
// new CameraUnlock.ini through the frozen import and is never written;
// tests/config_differential/ holds that to the published build over the whole
// corpus, and the cases here are the ones worth reading as examples.
//
// `yakuza0_config_tests --render-config <path>` writes the fresh render to
// <path> and exits, which is how `pixi run render-config` rewrites the committed
// file after a change to a row, a comment or a default.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <windows.h>

#include "mod_config.h"

namespace {

namespace cfg = ::cameraunlock::config;
namespace fs = std::filesystem;
using cameraunlock::TrackingMode;

int g_checks = 0;
int g_failures = 0;

void Check(bool ok, const std::string& what) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL %s\n", what.c_str());
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

std::string Rendered() {
    return cfg::RenderCanonicalFresh(yakuza0::config::Table(), yakuza0::config::Header());
}

std::string CommittedFile() { return ReadFileBytes(fs::path(YAKUZA0_SOURCE_DIR) / "CameraUnlock.ini"); }

// A folder of its own per case, removed afterwards: `game` stands for the folder
// holding the mod DLL, and Defaults.ini sits in `global` beside it.
class Scratch {
public:
    explicit Scratch(const char* tag) {
        wchar_t temp[MAX_PATH + 1] = {};
        if (GetTempPathW(MAX_PATH + 1, temp) == 0) throw std::runtime_error("GetTempPathW failed");
        root_ = fs::path(temp) /
                ("yakuza0_ht_config_" + std::string(tag) + "_" + std::to_string(GetCurrentProcessId()));
        fs::remove_all(root_);
        fs::create_directories(game());
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    // A scanner can still hold a file the test just wrote, and a destructor must
    // not throw, so a folder left behind is reported and the run carries on.
    ~Scratch() {
        std::error_code error;
        fs::remove_all(root_, error);
        if (error) std::printf("  scratch folder left behind: %s: %s\n", root_.string().c_str(), error.message().c_str());
    }

    fs::path game() const { return root_ / "game"; }
    fs::path ini() const { return game() / "CameraUnlock.ini"; }
    fs::path legacy() const { return game() / "Yakuza0HeadTracking.ini"; }
    fs::path defaults() const { return root_ / "global" / "Defaults.ini"; }

    yakuza0::Config Load() const {
        return yakuza0::config::Load(game().wstring(), cfg::DefaultsFile::At(defaults().wstring()));
    }

    std::set<std::string> Names() const {
        std::set<std::string> names;
        for (const auto& entry : fs::directory_iterator(game())) names.insert(entry.path().filename().string());
        return names;
    }

private:
    fs::path root_;
};

std::vector<std::string> Lines(const std::string& bytes) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t end; (end = bytes.find("\r\n", start)) != std::string::npos; start = end + 2) {
        lines.push_back(bytes.substr(start, end - start));
    }
    return lines;
}

// The lines that differ between two files of the same line count, or "count" when
// the counts differ.
std::vector<std::string> ChangedLines(const std::string& before, const std::string& after) {
    const std::vector<std::string> a = Lines(before), b = Lines(after);
    if (a.size() != b.size()) return {"count"};
    std::vector<std::string> changed;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) changed.push_back(b[i]);
    }
    return changed;
}

bool Holds(const std::string& bytes, const std::string& line) {
    return bytes.find("\r\n" + line + "\r\n") != std::string::npos;
}

void TheCommittedFileIsTheFreshRender() {
    Check(Rendered() == CommittedFile(), "CameraUnlock.ini is the table's fresh render; run pixi run render-config");
}

// Every row holds `default`: the mod has no row of its own and no collision.
void TheCommittedFileFollowsDefaultsIni() {
    const std::string committed = CommittedFile();
    for (const char* line :
         {"UdpPort=default", "EnableOnStartup=default", "WorldSpaceYaw=default", "RotationEnabled=default",
          "LocalSmoothing=default", "RemoteSmoothing=default", "PositionEnabled=default",
          "PositionLimitX=default", "PositionLimitY=default", "PositionLimitYDown=default", "PositionLimitZ=default",
          "PositionLimitZBack=default", "ToggleKey=default", "CycleTrackingModeKey=default", "YawModeKey=default"}) {
        Check(Holds(committed, line), std::string("the committed file holds ") + line);
    }
    Check(committed.find("Sensitivity") == std::string::npos && committed.find("Invert") == std::string::npos,
          "the committed file has no sensitivity or inversion");
    Check(committed.find("[Reticle]") == std::string::npos, "the committed file has no reticle setting");
}

void FirstLaunchCreatesTheCommittedFile() {
    Scratch s("created");
    const yakuza0::Config loaded = s.Load();
    Check(ReadFileBytes(s.ini()) == CommittedFile(), "the first launch writes the committed file byte for byte");
    Check(s.Names() == std::set<std::string>{"CameraUnlock.ini"},
          "the first launch creates CameraUnlock.ini and nothing else beside the mod");
    Check(fs::exists(s.defaults()), "the first launch creates Defaults.ini where none exists");
    Check(loaded.toggle_key == "End, Ctrl+Shift+Y", "ToggleKey starts at End, Ctrl+Shift+Y");
    Check(loaded.cycle_tracking_mode_key == "PageUp, Ctrl+Shift+G", "CycleTrackingModeKey starts at PageUp, Ctrl+Shift+G");
    Check(loaded.yaw_mode_key == "PageDown, Ctrl+Shift+H", "YawModeKey starts at PageDown, Ctrl+Shift+H");
    Check(loaded.udp_port == 4242, "the receiver listens on 4242");
    Check(loaded.enable_on_startup && loaded.world_space_yaw, "tracking starts on, in world-space yaw");
    Check(yakuza0::config::StartupTrackingMode(loaded) == TrackingMode::RotationAndPosition,
          "tracking starts in rotation and position");
    Check(loaded.local_smoothing == 0.0f && loaded.remote_smoothing == 0.15f, "smoothing starts at 0 local, 0.15 remote");
    Check(loaded.position_limit_z == 0.40f && loaded.position_limit_z_back == 0.10f,
          "the lean reaches 0.40 forward and 0.10 back");
}

// A value in Defaults.ini reaches every row holding `default`.
void ADefaultRowFollowsDefaultsIni() {
    Scratch s("follows");
    s.Load();
    WriteFileBytes(s.defaults(), "[CameraUnlock]\r\nConfigFormat=1\r\n\r\n[General]\r\nWorldSpaceYaw=false\r\n\r\n"
                                 "[Hotkeys]\r\nToggleKey=F8\r\n");
    const yakuza0::Config c = s.Load();
    Check(c.toggle_key == "F8", "ToggleKey follows Defaults.ini");
    Check(!c.world_space_yaw, "WorldSpaceYaw follows Defaults.ini");
}

void TheYawToggleSavesItsLineAndNothingElse() {
    Scratch s("save_yaw");
    s.Load();
    const std::string before = ReadFileBytes(s.ini());
    const std::string defaults = ReadFileBytes(s.defaults());
    yakuza0::config::SaveWorldSpaceYaw(false);
    Check(ChangedLines(before, ReadFileBytes(s.ini())) == std::vector<std::string>{"WorldSpaceYaw=false"},
          "a yaw save writes WorldSpaceYaw over default, and nothing else");
    Check(ReadFileBytes(s.defaults()) == defaults, "a save leaves Defaults.ini as it was");
    Check(!s.Load().world_space_yaw, "the saved yaw mode comes back at the next launch");
}

// The mode is one setting in two rows, so a save writes both.
void TheModeCycleSavesThePair() {
    Scratch s("save_mode");
    s.Load();
    const std::string before = ReadFileBytes(s.ini());

    yakuza0::config::SaveTrackingMode(TrackingMode::RotationOnly);
    Check(ChangedLines(before, ReadFileBytes(s.ini())) ==
              (std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=false"}),
          "rotation only writes the pair over default");

    yakuza0::config::SaveTrackingMode(TrackingMode::PositionOnly);
    Check(ChangedLines(before, ReadFileBytes(s.ini())) ==
              (std::vector<std::string>{"RotationEnabled=false", "PositionEnabled=true"}),
          "position only writes the pair");
    Check(yakuza0::config::StartupTrackingMode(s.Load()) == TrackingMode::PositionOnly,
          "the saved mode comes back at the next launch");

    yakuza0::config::SaveTrackingMode(TrackingMode::RotationAndPosition);
    Check(ChangedLines(before, ReadFileBytes(s.ini())) ==
              (std::vector<std::string>{"RotationEnabled=true", "PositionEnabled=true"}),
          "back to full, the pair holds values");
}

// The legacy file is imported into a new CameraUnlock.ini and left as it was.
void TheLegacyFileIsImportedAndLeftAsItWas() {
    Scratch s("import");
    const std::string legacy =
        "[General]\r\n; my note\r\nWorldSpaceYaw=0\r\nUnknownKey=1\r\n[Hotkeys]\r\nYawModeKey=0x2E\r\n";
    WriteFileBytes(s.legacy(), legacy);
    const yakuza0::Config c = s.Load();
    Check(!c.world_space_yaw, "WorldSpaceYaw=0 is carried");
    Check(c.yaw_mode_key == "Delete, Ctrl+Shift+H", "the yaw key is carried beside its chord");
    Check(c.toggle_key == "End, Ctrl+Shift+Y" && c.cycle_tracking_mode_key == "PageUp, Ctrl+Shift+G",
          "the keys the dev build bound in code are the defaults");
    Check(ReadFileBytes(s.legacy()) == legacy, "Yakuza0HeadTracking.ini keeps its bytes");
    Check((s.Names() == std::set<std::string>{"CameraUnlock.ini", "Yakuza0HeadTracking.ini"}),
          "the import creates CameraUnlock.ini and nothing else");
    const std::string migrated = ReadFileBytes(s.ini());
    for (const char* line : {"WorldSpaceYaw=false", "YawModeKey=Delete, Ctrl+Shift+H", "UdpPort=default",
                             "RotationEnabled=default", "PositionEnabled=default", "ToggleKey=default"}) {
        Check(Holds(migrated, line), std::string("the migrated file holds ") + line);
    }

    // Once CameraUnlock.ini exists, Yakuza0HeadTracking.ini is not read again.
    WriteFileBytes(s.legacy(), "[General]\r\nWorldSpaceYaw=1\r\n");
    Check(!s.Load().world_space_yaw, "the next launch reads CameraUnlock.ini, not Yakuza0HeadTracking.ini");
    Check(ReadFileBytes(s.ini()) == migrated, "the next launch writes nothing");
}

// A yaw key on Shift alone is unbound, and the player keeps the chord.
void ABareModifierYawKeyKeepsTheChord() {
    Scratch s("modifier");
    WriteFileBytes(s.legacy(), "[Hotkeys]\r\nYawModeKey=0x10\r\n");
    Check(s.Load().yaw_mode_key == "Ctrl+Shift+H", "YawModeKey=0x10 imports as the chord alone");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--render-config") == 0) {
        WriteFileBytes(argv[2], Rendered());
        return 0;
    }

    TheCommittedFileIsTheFreshRender();
    TheCommittedFileFollowsDefaultsIni();
    FirstLaunchCreatesTheCommittedFile();
    ADefaultRowFollowsDefaultsIni();
    TheYawToggleSavesItsLineAndNothingElse();
    TheModeCycleSavesThePair();
    TheLegacyFileIsImportedAndLeftAsItWas();
    ABareModifierYawKeyKeepsTheChord();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
