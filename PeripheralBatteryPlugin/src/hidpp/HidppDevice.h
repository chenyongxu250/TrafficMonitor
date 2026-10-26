#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "hidpp/HidDevice.h"
#include "hidpp/HidppProtocol.h"

// Capture for --verbose: tx = true for sent reports.
using HidppTrace = std::function<void(bool tx, const uint8_t* data, size_t length)>;

struct HidppBattery
{
    uint16_t feature = 0;
    uint8_t featureIndex = 0;       // For ReadBatteryAt in the next cycle
    std::optional<int> percentage;
    std::optional<int> millivolts;
    uint8_t rawStatus = 0;          // Charging/status byte or 0x1001 flags, raw
    // Only interpreted for 0x1001 (see DecodeVoltageFlags); 0x1000/0x1004 stay Unknown until such a
    // device is available for testing.
    hidpp::ChargeState charge = hidpp::ChargeState::Unknown;
};

// A HID++ 2.0 device behind an open long report channel (receiver slot or wired device).
class HidppDevice
{
public:
    HidppDevice(HidDevice& channel, uint8_t deviceIndex, HidppTrace trace = nullptr);

    struct Version
    {
        int major = 0;
        int minor = 0;
    };

    // IRoot.GetProtocolVersion to several device indices at once; collects responses until the
    // window ends. An idle device only responds after several hundred milliseconds – pinged one
    // after another, its response would be lost while waiting for the next index.
    // With shortChannel (usage 0x0001 of the same receiver) the window ends earlier: there, the
    // receiver immediately answers pings to empty slots and to itself (0xFF) with a HID++ 1.0
    // error (captured: code 0x08 for empty slots, 0x01 for 0xFF).
    static std::map<uint8_t, Version> PingAll(HidDevice& channel, HidDevice* shortChannel,
        const std::vector<uint8_t>& indices, DWORD windowMs, const HidppTrace& trace);

    // IRoot.GetFeature; nullopt if the device does not have the feature.
    std::optional<uint8_t> FindFeature(uint16_t featureId);

    // Feature 0x0005 (Device Name and Type).
    std::optional<std::wstring> ReadName();

    // Order as per Plan: 0x1004, 0x1000, 0x1001.
    std::optional<HidppBattery> ReadBattery();

    // Reads directly via an already known feature index (a single request).
    std::optional<HidppBattery> ReadBatteryAt(uint16_t feature, uint8_t featureIndex);

    const std::wstring& LastError() const { return lastError_; }

private:
    enum class Outcome { Ok, Timeout, DeviceError, IoError };

    Outcome Request(uint8_t featureIndex, uint8_t function, std::initializer_list<uint8_t> params,
        hidpp::Params& reply);

    // Like Request, but retried once on timeout (with a new software ID).
    Outcome RequestWithRetry(uint8_t featureIndex, uint8_t function, std::initializer_list<uint8_t> params,
        hidpp::Params& reply);

    // index = 0 with Ok means: feature not present (except for IRoot).
    Outcome LookupFeature(uint16_t featureId, uint8_t& index);

    HidDevice& channel_;
    uint8_t deviceIndex_;
    HidppTrace trace_;
    std::wstring lastError_;
};
