#pragma once

#include <string>

#include "battery/BatteryStatus.h"

class IBatteryProvider
{
public:
    virtual ~IBatteryProvider() = default;

    virtual bool Initialize() = 0;

    virtual BatteryStatus GetStatus() = 0;

    virtual void Refresh() = 0;

    virtual std::wstring GetProviderName() const = 0;
};
