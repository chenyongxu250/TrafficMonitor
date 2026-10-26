#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hidpp/HidppDevice.h"

struct LogitechDeviceReport
{
    uint16_t productId = 0;         // Receiver or wired device (USB PID)
    std::wstring product;           // USB product name of the collection
    uint8_t deviceIndex = 0;        // 1..6 = receiver slot, 0xFF = wired device
    int protocolMajor = 0;
    int protocolMinor = 0;
    std::wstring name;              // Read via HID++ (feature 0x0005)
    std::optional<HidppBattery> battery;
    bool fromCache = false;         // Name/feature index taken over from the previous cycle
    std::wstring error;
};

struct LogitechScanResult
{
    std::vector<LogitechDeviceReport> devices;
    size_t channels = 0;            // HID++ long channels found
    std::vector<std::wstring> notes; // e.g. channels that could not be opened
};

// Finds Logitech HID++ devices and reads name and battery.
//
// With cache (plugin), the scanner remembers name and battery feature per channel and device index;
// from the second cycle on, only a ping + one battery query go out. Less traffic also means fewer
// opportunities for mix-ups with G Hub, which queries in parallel. If the query via the
// remembered index fails, the entry is discarded and read again.
//
// Blocks (ping window 2 s). Only call from a worker thread; not thread-safe.
class LogitechScanner
{
public:
    explicit LogitechScanner(bool useCache);

    LogitechScanResult Scan(const HidppTrace& trace = nullptr);

private:
    struct Known
    {
        std::wstring name;
        uint16_t batteryFeature = 0;
        uint8_t batteryIndex = 0;
    };

    void ReadDevice(HidDevice& channel, const std::wstring& path, LogitechDeviceReport& report,
        const HidppTrace& trace);

    bool useCache_;
    std::map<std::pair<std::wstring, uint8_t>, Known> cache_;   // (channel path, device index)
};
