#include "bluetooth/BluetoothWatcher.h"

#include <map>
#include <mutex>

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;

namespace
{
    constexpr wchar_t kAepIsConnected[] = L"System.Devices.Aep.IsConnected";
}

struct BluetoothWatcher::Impl
{
    Trigger trigger;
    DeviceWatcher watcher{ nullptr };
    std::mutex mutex;
    std::map<std::wstring, std::wstring> names;     // Id -> Name
    std::map<std::wstring, bool> connected;         // Id -> last known state
};

BluetoothWatcher::BluetoothWatcher(Trigger trigger)
    : impl_(std::make_unique<Impl>())
{
    impl_->trigger = std::move(trigger);
}

// Never destroyed under the loader lock: the object lives until the process exits.
BluetoothWatcher::~BluetoothWatcher() = default;

bool BluetoothWatcher::Start()
{
    try
    {
        Impl* impl = impl_.get();
        auto requested = single_threaded_vector<hstring>({ hstring(kAepIsConnected) });
        impl->watcher = DeviceInformation::CreateWatcher(BluetoothDevice::GetDeviceSelectorFromPairingState(true),
            requested, DeviceInformationKind::AssociationEndpoint);

        impl->watcher.Added([impl](DeviceWatcher const&, DeviceInformation const& info) {
            try
            {
                std::lock_guard lock(impl->mutex);
                impl->names[std::wstring(info.Id())] = info.Name();
                if (auto value = info.Properties().TryLookup(kAepIsConnected))
                    impl->connected[std::wstring(info.Id())] = unbox_value<bool>(value);
            }
            catch (...)
            {
            }
        });

        impl->watcher.Updated([impl](DeviceWatcher const&, DeviceInformationUpdate const& update) {
            try
            {
                auto value = update.Properties().TryLookup(kAepIsConnected);
                if (!value)
                    return;
                const bool isConnected = unbox_value<bool>(value);
                std::wstring name;
                {
                    std::lock_guard lock(impl->mutex);
                    auto& known = impl->connected[std::wstring(update.Id())];
                    if (known == isConnected && impl->names.count(std::wstring(update.Id())))
                        return;     // Update without a change
                    known = isConnected;
                    name = impl->names[std::wstring(update.Id())];
                }
                impl->trigger(L"Bluetooth: " + (name.empty() ? std::wstring(L"device") : name)
                    + (isConnected ? L" connected" : L" disconnected"));
            }
            catch (...)
            {
            }
        });

        impl->watcher.Start();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::optional<bool> BluetoothWatcher::EnsureRunning()
{
    try
    {
        if (!impl_->watcher)
            return Start();
        switch (impl_->watcher.Status())
        {
        case DeviceWatcherStatus::Started:
        case DeviceWatcherStatus::EnumerationCompleted:
        case DeviceWatcherStatus::Stopping:     // Becomes Stopped shortly, then in the next cycle
            return std::nullopt;
        default:
            // Created, Stopped, Aborted: in these states the API allows Start(); the
            // event handlers stay registered.
            impl_->watcher.Start();
            return true;
        }
    }
    catch (...)
    {
        return false;
    }
}
