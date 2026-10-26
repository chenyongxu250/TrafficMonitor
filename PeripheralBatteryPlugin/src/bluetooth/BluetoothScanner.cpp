#include "bluetooth/BluetoothScanner.h"

#include <winerror.h>

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
    constexpr wchar_t kAepDeviceAddress[] = L"System.Devices.Aep.DeviceAddress";
    constexpr wchar_t kAepContainerId[] = L"System.Devices.Aep.ContainerId";
    constexpr wchar_t kBatteryKey[] = L"{104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2";

    std::wstring PropertyTypeName(PropertyType type)
    {
        switch (type)
        {
        case PropertyType::UInt8: return L"UInt8";
        case PropertyType::UInt16: return L"UInt16";
        case PropertyType::UInt32: return L"UInt32";
        case PropertyType::Int32: return L"Int32";
        case PropertyType::Boolean: return L"Boolean";
        case PropertyType::String: return L"String";
        case PropertyType::Guid: return L"Guid";
        default: return L"Type" + std::to_wstring(static_cast<int>(type));
        }
    }

    // Waits at most 10 s for a device query. If it hung, the worker (and with it both displays)
    // would otherwise be stuck forever; this way it is cancelled and reported as an error.
    template <typename Result>
    Result Await(IAsyncOperation<Result> const& operation)
    {
        if (operation.wait_for(std::chrono::seconds(10)) == AsyncStatus::Started)
        {
            operation.Cancel();
            throw hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT), L"device query timed out");
        }
        return operation.GetResults();
    }

    IPropertyValue Lookup(DeviceInformation const& info, wchar_t const* key)
    {
        auto value = info.Properties().TryLookup(key);
        return value ? value.try_as<IPropertyValue>() : nullptr;
    }

    std::optional<bool> ReadBool(DeviceInformation const& info, wchar_t const* key)
    {
        auto pv = Lookup(info, key);
        if (!pv || pv.Type() != PropertyType::Boolean)
            return std::nullopt;
        return pv.GetBoolean();
    }

    std::wstring ReadString(DeviceInformation const& info, wchar_t const* key)
    {
        auto pv = Lookup(info, key);
        if (!pv || pv.Type() != PropertyType::String)
            return {};
        return std::wstring(pv.GetString());
    }

    std::wstring ReadGuid(DeviceInformation const& info, wchar_t const* key)
    {
        auto pv = Lookup(info, key);
        if (!pv || pv.Type() != PropertyType::Guid)
            return {};
        return std::wstring(to_hstring(pv.GetGuid()));
    }

    void ReadContainerNodes(BluetoothDeviceReport& report)
    {
        if (report.containerId.empty())
            return;

        std::wstring aqs = L"System.Devices.ContainerId:=\"" + report.containerId + L"\"";
        auto requested = single_threaded_vector<hstring>({ hstring(kBatteryKey) });
        auto nodes = Await(DeviceInformation::FindAllAsync(aqs, requested, DeviceInformationKind::Device));

        for (auto const& node : nodes)
        {
            BluetoothNode entry;
            entry.name = node.Name();
            entry.id = node.Id();
            if (auto pv = Lookup(node, kBatteryKey))
            {
                entry.batteryType = PropertyTypeName(pv.Type());
                if (pv.Type() == PropertyType::UInt8)
                    entry.battery = pv.GetUInt8();
            }
            if (entry.battery && !report.battery)
            {
                report.battery = entry.battery;
                report.batterySource = entry.name;
            }
            report.nodes.push_back(std::move(entry));
        }
    }
}

BluetoothScanResult ScanBluetoothDevices()
{
    BluetoothScanResult result;
    try
    {
        auto requested = single_threaded_vector<hstring>(
            { hstring(kAepIsConnected), hstring(kAepDeviceAddress), hstring(kAepContainerId) });

        struct Source { hstring selector; wchar_t const* transport; };
        Source sources[] = {
            { BluetoothDevice::GetDeviceSelectorFromPairingState(true), L"classic" },
            { BluetoothLEDevice::GetDeviceSelectorFromPairingState(true), L"le" },
        };

        for (auto const& source : sources)
        {
            auto devices = Await(DeviceInformation::FindAllAsync(
                source.selector, requested, DeviceInformationKind::AssociationEndpoint));

            for (auto const& device : devices)
            {
                BluetoothDeviceReport report;
                report.name = device.Name();
                report.transport = source.transport;
                report.connected = ReadBool(device, kAepIsConnected);
                report.address = ReadString(device, kAepDeviceAddress);
                report.containerId = ReadGuid(device, kAepContainerId);
                try
                {
                    ReadContainerNodes(report);
                }
                catch (hresult_error const& e)
                {
                    report.error = L"container query failed: " + std::wstring(e.message());
                }
                result.devices.push_back(std::move(report));
            }
        }
    }
    catch (hresult_error const& e)
    {
        wchar_t code[16];
        swprintf_s(code, L"0x%08X", static_cast<unsigned>(e.code()));
        result.error = std::wstring(L"device enumeration failed (") + code + L"): " + std::wstring(e.message());
    }
    return result;
}
