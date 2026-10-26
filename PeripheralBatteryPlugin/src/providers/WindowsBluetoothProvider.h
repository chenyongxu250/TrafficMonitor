#pragma once

#include "battery/IBatteryProvider.h"

// Battery level of a paired Bluetooth device, selected by a part of its name.
// Refresh() blocks (WinRT) and may only be called from the BatteryManager's worker thread.
class WindowsBluetoothProvider : public IBatteryProvider
{
public:
    explicit WindowsBluetoothProvider(std::wstring nameContains);

    bool Initialize() override;
    BatteryStatus GetStatus() override;
    void Refresh() override;
    std::wstring GetProviderName() const override;

private:
    std::wstring nameContains_;
    BatteryStatus status_;
};
