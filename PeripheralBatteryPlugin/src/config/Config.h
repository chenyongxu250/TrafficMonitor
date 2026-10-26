#pragma once

#include <string>

// Settings from PeripheralBattery.ini (Plan §32). Missing keys = default values.
// Labels: with Colors=1 the plugin draws the item itself, and TrafficMonitor does not pass on its
// label from the display settings in that case → Label= in [Headphones]/[Mouse].
// With Colors=0 TrafficMonitor draws, and its label applies.

struct DisplayOptions
{
    bool showDisconnected = true;   // "—" for a disconnected device, otherwise empty
    bool showPercentSign = true;    // "82%" instead of "82"
    bool showCharging = true;       // "⚡" while charging
    bool colors = true;             // value red on low battery, green while charging (plugin draws itself)
    std::wstring headphonesLabel = L"M4";
    std::wstring mouseLabel = L"Mouse";

    bool operator==(const DisplayOptions&) const = default;
};

struct DeviceConfig
{
    bool enabled = true;
    std::wstring provider;          // "WindowsBluetooth" or "LogitechHidpp"
    std::wstring nameContains;      // part of the device name, case-insensitive

    bool operator==(const DeviceConfig&) const = default;
};

struct Config
{
    static constexpr int kDefaultInterval = 60;
    static constexpr int kMinInterval = 15;
    static constexpr int kMaxInterval = 3600;

    int updateIntervalSeconds = kDefaultInterval;
    int lowBatteryWarning = 15;     // notification at <= this value; 0 = off
    bool verboseLogging = false;
    DisplayOptions display;
    DeviceConfig headphones{ true, L"WindowsBluetooth", L"MOMENTUM 4" };
    DeviceConfig mouse{ true, L"LogitechHidpp", L"" };

    bool operator==(const Config&) const = default;
};

// Reads the file; if it is missing, the default values are returned.
Config LoadConfig(const std::wstring& path);

// Creates the file with default values and comments: writes config/PeripheralBattery.ini, which is
// embedded in the DLL as a resource (UTF-16 LE with BOM, so that GetPrivateProfileStringW also
// reads non-ASCII names). Overwrites nothing.
bool WriteDefaultConfigIfMissing(const std::wstring& path);

// Adds keys that are missing from an older file, with their default value (without comment).
// Existing values stay untouched. Returns: number of keys added.
int AddMissingKeys(const std::wstring& path);

// Writes all settings (options dialog). The file's comments are preserved; if it is missing, the
// default file is created first (otherwise an ANSI file would result). On false, `error` holds
// the Windows error message (e.g. access denied under C:\Program Files).
bool SaveConfig(const std::wstring& path, const Config& config, std::wstring& error);
