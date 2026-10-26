#pragma once

#include <PluginInterface.h>

#include "battery/BatteryManager.h"
#include "items/HeadphonesBatteryItem.h"
#include "items/MouseBatteryItem.h"
#include "utils/UiNotifier.h"

class PeripheralBatteryPlugin : public ITMPlugin
{
public:
    static PeripheralBatteryPlugin& Instance();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    const wchar_t* GetTooltipInfo() override;
    void OnInitialize(ITrafficMonitor* pApp) override;
    OptionReturn ShowOptionsDialog(void* hParent) override;

private:
    PeripheralBatteryPlugin() = default;

    UiNotifier notifier_;
    std::wstring tooltip_;      // UI thread only (sole caller of GetTooltipInfo)
    DrawContext drawContext_;   // UI thread only (OnExtenedInfo before DrawItem)
    bool optionsOpen_ = false;  // UI thread only; without an owner window the dialog could be opened twice

    // Order matters: the items hold references to manager_ and drawContext_.
    BatteryManager manager_;
    HeadphonesBatteryItem headphones_{ manager_, drawContext_ };
    MouseBatteryItem mouse_{ manager_, drawContext_ };
};

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
