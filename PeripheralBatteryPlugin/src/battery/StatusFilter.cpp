#include "battery/StatusFilter.h"

BatteryStatus StatusFilter::Apply(BatteryStatus fresh, std::chrono::seconds minOutage, Clock::time_point now)
{
    if (fresh.available)
    {
        failures_ = 0;
        lastGood_ = fresh;
        return fresh;
    }

    if (fresh.state == BatteryState::Disconnected)
    {
        failures_ = 0;
        lastGood_ = BatteryStatus{};
        return fresh;
    }

    if (failures_ == 0)
        firstFailure_ = now;
    ++failures_;
    if (failures_ >= kFailureThreshold && now - firstFailure_ >= minOutage)
    {
        fresh.state = BatteryState::Disconnected;
        return fresh;
    }
    if (lastGood_.available)
    {
        // Show the last value, but pass along the current error for diagnostics.
        BatteryStatus kept = lastGood_;
        kept.error = fresh.error;
        return kept;
    }
    return fresh;   // no valid value known yet → "?"
}

void StatusFilter::Reset()
{
    lastGood_ = BatteryStatus{};
    failures_ = 0;
    firstFailure_ = {};
}
