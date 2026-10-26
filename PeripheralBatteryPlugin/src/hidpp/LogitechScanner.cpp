#include "hidpp/LogitechScanner.h"

#include <set>

#include "hidpp/HidDevice.h"
#include "hidpp/HidppProtocol.h"

namespace
{
    constexpr uint16_t kLogitechVendorId = 0x046D;

    // On the test machine the idle G502 only responded after 300–900 ms. With the short channel the
    // window ends as soon as all indices have been answered (empty slots report back immediately).
    constexpr DWORD kPingWindowMs = 2000;
}

LogitechScanner::LogitechScanner(bool useCache)
    : useCache_(useCache)
{
}

void LogitechScanner::ReadDevice(HidDevice& channel, const std::wstring& path, LogitechDeviceReport& report,
    const HidppTrace& trace)
{
    HidppDevice device(channel, report.deviceIndex, trace);
    const auto key = std::make_pair(path, report.deviceIndex);

    if (useCache_)
    {
        if (auto it = cache_.find(key); it != cache_.end())
        {
            // First check that the battery feature is still at the remembered index – otherwise, after
            // re-pairing, the response of an unrelated feature could be interpreted as a battery value.
            const Known& known = it->second;
            if (device.FindFeature(known.batteryFeature) == known.batteryIndex)
                report.battery = device.ReadBatteryAt(known.batteryFeature, known.batteryIndex);
            if (report.battery)
            {
                report.name = known.name;
                // If the name failed on first contact (e.g. right after waking up, while G Hub floods
                // the mouse with requests), try again now – otherwise a name filter would never match
                // this device as long as it is in the cache.
                if (report.name.empty())
                {
                    if (auto name = device.ReadName())
                        it->second.name = report.name = *name;
                }
                report.fromCache = true;
                return;
            }
            cache_.erase(it);   // Different device in the slot or no response: read everything again
        }
    }

    if (auto name = device.ReadName())
        report.name = *name;
    report.battery = device.ReadBattery();
    if (!report.battery)
    {
        report.error = device.LastError();
        return;
    }
    if (useCache_)
        cache_[key] = Known{ report.name, report.battery->feature, report.battery->featureIndex };
}

LogitechScanResult LogitechScanner::Scan(const HidppTrace& trace)
{
    LogitechScanResult result;
    std::set<std::pair<std::wstring, uint8_t>> seen;

    const std::vector<HidDeviceInfo> all = EnumerateHidDevices(kLogitechVendorId);
    for (const HidDeviceInfo& info : all)
    {
        if (info.usagePage != hidpp::kUsagePage || info.usage != hidpp::kLongUsage
            || info.outputReportLength != hidpp::kLongReportLength)
        {
            continue;
        }
        ++result.channels;

        // Short channel of the same receiver (usage 0x0001) for the fast error responses.
        // With two receivers sharing a PID the first one is taken; that then only costs time.
        HidDevice shortChannel;
        for (const HidDeviceInfo& other : all)
        {
            if (other.productId == info.productId && other.usagePage == hidpp::kUsagePage
                && other.usage == hidpp::kShortUsage && shortChannel.Open(other.path))
            {
                break;
            }
        }

        HidDevice channel;
        if (!channel.Open(info.path))
        {
            wchar_t note[96];
            swprintf_s(note, L"PID %04X: open failed (%lu)", info.productId, GetLastError());
            result.notes.push_back(note);
            continue;
        }

        // 0xFF = wired device (answers itself with HID++ 2.0), 1..6 = receiver slots; the index is
        // not always 1 (Plan §22). Ping all at once, collect responses in the shared window.
        std::vector<uint8_t> indices{ hidpp::kCordedIndex };
        for (uint8_t slot = hidpp::kFirstReceiverSlot; slot <= hidpp::kLastReceiverSlot; ++slot)
            indices.push_back(slot);
        const auto answered = HidppDevice::PingAll(channel, shortChannel.IsOpen() ? &shortChannel : nullptr,
            indices, kPingWindowMs, trace);

        std::vector<uint8_t> devices;
        if (auto corded = answered.find(hidpp::kCordedIndex); corded != answered.end() && corded->second.major >= 2)
        {
            devices.push_back(hidpp::kCordedIndex);
        }
        else
        {
            for (const auto& [index, version] : answered)
            {
                if (index != hidpp::kCordedIndex && version.major >= 2)
                    devices.push_back(index);
            }
        }

        for (uint8_t index : devices)
        {
            const HidppDevice::Version& version = answered.at(index);
            LogitechDeviceReport report;
            report.productId = info.productId;
            report.product = info.product;
            report.deviceIndex = index;
            report.protocolMajor = version.major;
            report.protocolMinor = version.minor;
            ReadDevice(channel, info.path, report, trace);
            seen.insert({ info.path, index });
            result.devices.push_back(std::move(report));
        }
    }

    // Discard entries for devices that did not respond this time: they are read again on the next
    // contact (there could be a different device in the slot).
    std::erase_if(cache_, [&](const auto& entry) { return !seen.contains(entry.first); });
    return result;
}
