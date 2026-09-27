#pragma once

#include <cameraunlock/logging/file_log.h>

// The process-wide log lives in cameraunlock-core (log::Open/Close/Line/
// EmergencyLine). Aliased so call sites read log::Line(...) unqualified.
namespace yakuza0 {
namespace log = ::cameraunlock::logging;
}
