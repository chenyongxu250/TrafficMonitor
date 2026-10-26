#include "providers/WindowsBluetoothProvider.h"

#include <algorithm>
#include <cwctype>

#include "bluetooth/BluetoothScanner.h"

namespace
{
    std::wstring ToLower(std::wstring text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return text;
    }

    bool IsConnected(const BluetoothDeviceReport& device)
    {
        return device.connected.value_or(false);
    }
}

WindowsBluetoothProvider::WindowsBluetoothProvider(std::wstring nameContains)
    : nameContains_(std::move(nameContains))
{
    status_.deviceName = nameContains_;
}

bool WindowsBluetoothProvider::Initialize()
{
    return true;
}

BatteryStatus WindowsBluetoothProvider::GetStatus()
{
    return status_;
}

void WindowsBluetoothProvider::Refresh()
{
    BatteryStatus status;
    status.deviceName = nameContains_;
    status.lastUpdate = std::chrono::system_clock::now();

    BluetoothScanResult scan = ScanBluetoothDevices();
    if (!scan.error.empty())
    {
        // Query failed, nothing known about the device: transient (StatusFilter).
        status.state = BatteryState::Error;
        status.error = scan.error;
        status_ = std::move(status);
        return;
    }

    // If several devices match, a connected one wins.
    const std::wstring needle = ToLower(nameContains_);
    const BluetoothDeviceReport* match = nullptr;
    for (const auto& device : scan.devices)
    {
        if (needle.empty())
        {
            // Without a name filter, every paired device would match (e.g. a speaker without a
            // battery value): only connected devices for which Windows knows a battery level.
            if (!IsConnected(device) || !device.battery)
                continue;
        }
        else if (ToLower(device.name).find(needle) == std::wstring::npos)
        {
            continue;
        }
        if (match == nullptr || (IsConnected(device) && !IsConnected(*match)))
            match = &device;
    }

    if (match == nullptr)
    {
        status.state = BatteryState::Disconnected;
        status.error = needle.empty() ? std::wstring(L"no connected device with battery level")
                                      : L"no paired device matches '" + nameContains_ + L"'";
    }
    else
    {
        status.deviceName = match->name;
        status.deviceId = match->containerId;
        status.detail = L"Bluetooth " + match->transport
            + (match->batterySource.empty() ? L"" : L", node " + match->batterySource);

        // Connection first: Windows keeps the last battery value for a disconnected device.
        if (!IsConnected(*match))
        {
            status.state = BatteryState::Disconnected;
        }
        else if (match->battery)
        {
            status.available = true;
            status.percentage = *match->battery;
            // The source has no charge state; Discharging here only means "value available".
            status.state = BatteryState::Discharging;
        }
        else
        {
            status.state = BatteryState::Unknown;
            status.error = L"connected, but no battery property";
        }
    }
    status_ = std::move(status);
}

std::wstring WindowsBluetoothProvider::GetProviderName() const
{
    return L"WindowsBluetooth";
}
