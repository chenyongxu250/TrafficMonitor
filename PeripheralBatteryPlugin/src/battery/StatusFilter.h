#pragma once

#include <chrono>

#include "battery/BatteryStatus.h"

// Dropout filter per Plan §39: a device that does not respond to a single query should not
// immediately show "—" and flicker.
//
// - Valid value: shown and remembered.
// - Disconnected: applies immediately. Providers only report it when it is established (Windows
//   reports the Bluetooth device as disconnected, the Logitech receiver reports the mouse as
//   disconnected, no receiver present).
// - Error/Unknown (e.g. mouse not responding, query failed): the last valid value stays until
//   at least kFailureThreshold consecutive failures have occurred AND minOutage has passed since
//   the first one; after that Disconnected. The time condition prevents immediate queries
//   (events) from racking up three failures within a few seconds.
class StatusFilter
{
public:
    using Clock = std::chrono::steady_clock;

    static constexpr int kFailureThreshold = 3;

    BatteryStatus Apply(BatteryStatus fresh, std::chrono::seconds minOutage, Clock::time_point now = Clock::now());

    void Reset();

private:
    BatteryStatus lastGood_;
    int failures_ = 0;
    Clock::time_point firstFailure_{};
};
