#pragma once

#include <chrono>
#include <string>

enum class BatteryState
{
    Unknown,
    Disconnected,
    Discharging,
    Charging,
    Full,
    Error
};

struct BatteryStatus
{
    bool available = false;

    // -1 = unknown. 0 is a valid battery level.
    int percentage = -1;

    BatteryState state = BatteryState::Unknown;

    std::wstring deviceName;

    std::wstring deviceId;

    std::chrono::system_clock::time_point lastUpdate;

    std::wstring error;

    // Technical details from the source for log and diagnostics (e.g. node, slot, millivolts).
    std::wstring detail;

    // Measured voltage if the source only reports voltage (Logitech 0x1001); otherwise -1.
    int millivolts = -1;
};
