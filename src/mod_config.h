#pragma once

#include <string>

#include <cameraunlock/config/config_concepts.g.h>
#include <cameraunlock/config/config_owner.h>
#include <cameraunlock/config/defaults_file.h>
#include <cameraunlock/config/legacy_import.h>
#include <cameraunlock/data/position_settings.h>
#include <cameraunlock/math/smoothing_utils.h>
#include <cameraunlock/tracking/tracking_mode.h>

namespace yakuza0 {

// The settings CameraUnlock.ini holds, at their defaults.
struct Config {
    int udp_port = 4242;
    bool enable_on_startup = true;

    // true = yaw turns about the world up-axis (horizon-locked). false = yaw
    // turns about the camera's own up-axis.
    bool world_space_yaw = true;

    // The tracking mode at startup, the pair the mode hotkey saves.
    bool rotation_enabled = true;
    bool position_enabled = true;

    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    float position_limit_x = cameraunlock::PositionSettings{}.limit_x;
    float position_limit_y = cameraunlock::PositionSettings{}.limit_y;
    float position_limit_y_down = cameraunlock::PositionSettings{}.limit_y_down;
    float position_limit_z = cameraunlock::PositionSettings{}.limit_z;
    float position_limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;

    std::string toggle_key =
        cameraunlock::config::schema::ConceptTraits<cameraunlock::config::schema::Concept::ToggleKey>::kCanonicalDefault;
    std::string cycle_tracking_mode_key =
        cameraunlock::config::schema::ConceptTraits<cameraunlock::config::schema::Concept::CycleTrackingModeKey>::kCanonicalDefault;
    std::string yaw_mode_key =
        cameraunlock::config::schema::ConceptTraits<cameraunlock::config::schema::Concept::YawModeKey>::kCanonicalDefault;
};

// Absolute path of the mod's log file, next to the mod DLL.
std::wstring LogFilePath();
std::wstring PrevLogFilePath();

// The folder the mod DLL sits in (the game's media\ folder), with no trailing
// backslash.
std::wstring ModuleDir();

}  // namespace yakuza0

// CameraUnlock.ini, beside the mod DLL, in cameraunlock-core's canonical config
// format. One ConfigOwner reads and writes it; nothing else in the mod touches
// it. Yakuza0HeadTracking.ini, the file the dev build read, is imported once
// while CameraUnlock.ini is absent and is never written.
namespace yakuza0::config {

cameraunlock::config::ConfigTable<Config> Table();

cameraunlock::config::RenderHeader Header();

// Yakuza0HeadTracking.ini through the frozen reader in src/legacy_config/,
// mapped into Config.
cameraunlock::config::LegacyImport<Config> Import();

// The owner's options for CameraUnlock.ini in `dir`, a full path, with
// Yakuza0HeadTracking.ini beside it as the legacy file and Defaults.ini where
// `defaults` says.
cameraunlock::config::ConfigOwnerOptions<Config> OwnerOptions(const std::wstring& dir,
                                                              cameraunlock::config::DefaultsFile defaults);

// Reads, imports or creates CameraUnlock.ini in `dir`, logs what the owner
// reports, and returns the settings the session runs on. Call once, from the
// init thread, with the log open. `defaults` is DefaultsFile::PerUser() in the
// mod.
Config Load(const std::wstring& dir, cameraunlock::config::DefaultsFile defaults);

// The tracking mode the settings start in. The table never gives both rows
// false.
cameraunlock::TrackingMode StartupTrackingMode(const Config& config);

// Saves the value a hotkey has just applied. The session keeps it whether or
// not the save succeeds; a failed save is logged. Called on the hotkey thread.
void SaveWorldSpaceYaw(bool world_space_yaw);
void SaveTrackingMode(cameraunlock::TrackingMode mode);

}  // namespace yakuza0::config
