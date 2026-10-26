#pragma once

#include "items/BatteryItem.h"

class HeadphonesBatteryItem : public BatteryItem
{
public:
    using BatteryItem::BatteryItem;

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;

protected:
    BatteryStatus ReadStatus() const override;
    std::wstring ReadLabel(const DisplayOptions& options) const override;
};
