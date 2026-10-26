#pragma once

#include <windows.h>
#include <cfgmgr32.h>

#include <functional>
#include <mutex>
#include <set>
#include <string>

#include "hidpp/LinkRegistry.h"

// Listens permanently and passively (sends nothing) on the HID++ channels of the Logitech receivers
// and reports when an immediate query is worthwhile. Everything captured on LIGHTSPEED C539 + G502:
// - short channel, receiver notification 0x41: device connected/disconnected → LinkRegistry + trigger
// - long channel, event (software ID 0) from the slot's battery feature index: charging cable, battery
//   value, reconnect → trigger. Other events (e.g. buttons on index 0x0A, very frequent) and the
//   traffic of other programs (G Hub) are ignored.
// - Windows reports a Logitech HID device arrival/removal (receiver plugged in/unplugged) → trigger;
//   Refresh() opens new channels in the next cycle.
//
// Lifetime: until the process exits. The read threads are not joined and the Windows registration
// is not removed – TrafficMonitor unloads plugins on exit under the loader lock (see
// BatteryManager::StartWorker); the worker keeps the DLL loaded until then.
class HidppListener
{
public:
    using Trigger = std::function<void(std::wstring reason)>;

    HidppListener(hidpp::LinkRegistry& registry, Trigger trigger);

    HidppListener(const HidppListener&) = delete;
    HidppListener& operator=(const HidppListener&) = delete;

    // Opens channels that are not being listened to yet; call from the worker thread.
    void Refresh();

private:
    void ReadLoop(std::wstring path, uint16_t productId, uint16_t usage, uint16_t inputLength);
    void HandleShort(uint16_t productId, const uint8_t* report);
    void HandleLong(uint16_t productId, const uint8_t* report);
    static DWORD CALLBACK OnDeviceChange(HCMNOTIFICATION, PVOID context, CM_NOTIFY_ACTION action,
        PCM_NOTIFY_EVENT_DATA data, DWORD);

    hidpp::LinkRegistry& registry_;
    Trigger trigger_;
    std::mutex mutex_;
    std::set<std::wstring> listening_;      // Paths with a running read thread
    HCMNOTIFICATION notification_ = nullptr;
};
