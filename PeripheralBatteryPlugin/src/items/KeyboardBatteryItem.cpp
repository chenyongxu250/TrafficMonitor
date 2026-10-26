#include "items/KeyboardBatteryItem.h"

#include "battery/BatteryManager.h"

const wchar_t* KeyboardBatteryItem::GetItemName() const
{
    return L"Keyboard Battery";
}

const wchar_t* KeyboardBatteryItem::GetItemId() const
{
    return L"PeripheralBatteryKeyboard";
}

const wchar_t* KeyboardBatteryItem::GetItemLableText() const
{
    return L"Keyboard ";
}

BatteryStatus KeyboardBatteryItem::ReadStatus() const
{
    return manager_.GetKeyboardStatus();
}

std::wstring KeyboardBatteryItem::ReadLabel(const DisplayOptions& options) const
{
    return options.keyboardLabel;
}
