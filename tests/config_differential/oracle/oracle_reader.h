#pragma once

#include <string>
#include <tuple>
#include <vector>

// The oracle: what the dev pre-release (24bc0ea), the newest and only published
// build, ran on after reading Yakuza0HeadTracking.ini. oracle_reader.cpp
// compiles its reader and transcribes the startup code that consumed it.
namespace yakuza0_oracle {

enum Action { kToggle = 0, kCycleMode = 1, kYawMode = 2 };

// One HotkeyPoller registration: the action, the code, and 3 where the callback
// is ChordGuarded (fires only while Ctrl and Shift are both held), 0 where it is
// NavGuarded (fires unless Ctrl and Shift are both held).
using Registration = std::tuple<int, int, unsigned>;

struct Published {
    int udp_port = 0;
    bool tracking_enabled = false;
    bool world_space_yaw = false;
    // cameraunlock::TrackingMode's numbers: 0 rotation and position.
    int tracking_mode = 0;
    float local_smoothing = 0, remote_smoothing = 0;
    // The lean budgets in the pipeline's own directions: forward and back are
    // what the player leans, whichever way the engine's z runs.
    float limit_x = 0, limit_y = 0, limit_y_down = 0, limit_forward = 0, limit_back = 0;
    std::vector<Registration> hotkeys;
};

// `dir` is the folder the mod DLL sits in, with no trailing backslash. Like the
// dev build, this writes the default file there first when none exists.
Published Read(const std::string& dir);

}  // namespace yakuza0_oracle
