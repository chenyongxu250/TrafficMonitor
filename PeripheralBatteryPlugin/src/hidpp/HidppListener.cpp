#include "hidpp/HidppListener.h"

#include <hidsdi.h>

#include <cwctype>
#include <thread>

#include "hidpp/HidDevice.h"
#include "hidpp/HidppProtocol.h"

namespace
{
    constexpr uint16_t kLogitechVendorId = 0x046D;

    std::wstring Hex4(unsigned value)
    {
        wchar_t buffer[8];
        swprintf_s(buffer, L"%04X", value);
        return buffer;
    }
}

HidppListener::HidppListener(hidpp::LinkRegistry& registry, Trigger trigger)
    : registry_(registry)
    , trigger_(std::move(trigger))
{
    CM_NOTIFY_FILTER filter{};
    filter.cbSize = sizeof(filter);
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    HidD_GetHidGuid(&filter.u.DeviceInterface.ClassGuid);
    if (CM_Register_Notification(&filter, this, &HidppListener::OnDeviceChange, &notification_) != CR_SUCCESS)
        notification_ = nullptr;
}

void HidppListener::Refresh()
{
    for (const HidDeviceInfo& info : EnumerateHidDevices(kLogitechVendorId))
    {
        if (info.usagePage != hidpp::kUsagePage
            || (info.usage != hidpp::kShortUsage && info.usage != hidpp::kLongUsage))
        {
            continue;
        }
        {
            std::lock_guard lock(mutex_);
            if (!listening_.insert(info.path).second)
                continue;
        }
        try
        {
            std::thread(&HidppListener::ReadLoop, this, info.path, info.productId, info.usage, info.inputReportLength)
                .detach();
        }
        catch (...)
        {
            // Thread could not be started: do not track the channel as listened to; retry in the next cycle.
            std::lock_guard lock(mutex_);
            listening_.erase(info.path);
        }
    }
}

void HidppListener::ReadLoop(std::wstring path, uint16_t productId, uint16_t usage, uint16_t inputLength)
{
    try
    {
        HidDevice channel;
        if (channel.Open(path) && inputLength > 0 && inputLength <= 64)
        {
            for (;;)
            {
                // Exactly one report length: otherwise Windows returns several reports in one go.
                uint8_t report[64] = {};
                const int n = channel.Read(report, inputLength, 60000);
                if (n < 0)
                    break;          // Channel gone (receiver unplugged)
                if (n < 7)
                    continue;
                if (usage == hidpp::kShortUsage)
                    HandleShort(productId, report);
                else
                    HandleLong(productId, report);
            }
        }
    }
    catch (...)
    {
    }
    std::lock_guard lock(mutex_);
    listening_.erase(path);
}

void HidppListener::HandleShort(uint16_t productId, const uint8_t* report)
{
    if (report[0] != hidpp::kShortReportId || report[2] != hidpp::kConnectionNotification)
        return;
    const uint8_t slot = report[1];
    if (slot < hidpp::kFirstReceiverSlot || slot > hidpp::kLastReceiverSlot)
        return;
    const bool connected = (report[4] & hidpp::kLinkDownFlag) == 0;
    registry_.SetLink(productId, slot, connected);
    trigger_(L"Receiver " + Hex4(productId) + L": slot " + std::to_wstring(slot)
        + (connected ? L" connected" : L" disconnected"));
}

void HidppListener::HandleLong(uint16_t productId, const uint8_t* report)
{
    if (report[0] != hidpp::kLongReportId || (report[3] & 0x0F) != 0)
        return;     // Only events (software ID 0), no responses
    const uint8_t slot = report[1];
    const auto batteryIndex = registry_.GetBatteryIndex(productId, slot);
    if (!batteryIndex || report[2] != *batteryIndex)
        return;
    trigger_(L"Battery event from " + Hex4(productId) + L" slot " + std::to_wstring(slot));
}

DWORD CALLBACK HidppListener::OnDeviceChange(HCMNOTIFICATION, PVOID context, CM_NOTIFY_ACTION action,
    PCM_NOTIFY_EVENT_DATA data, DWORD)
{
    if (action != CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL && action != CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
        return ERROR_SUCCESS;
    try
    {
        std::wstring link = data->u.DeviceInterface.SymbolicLink;
        for (auto& c : link)
            c = static_cast<wchar_t>(std::towlower(c));
        if (link.find(L"vid_046d") == std::wstring::npos)
            return ERROR_SUCCESS;
        auto* self = static_cast<HidppListener*>(context);
        self->trigger_(action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL ? L"Logitech HID device plugged in"
                                                                          : L"Logitech HID device removed");
    }
    catch (...)
    {
    }
    return ERROR_SUCCESS;
}
