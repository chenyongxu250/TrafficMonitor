#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <utility>

// Shared state between HidppListener (events) and LogitechHidppProvider (query):
// - connection state per receiver slot from the receiver's HID++ 1.0 notification 0x41
// - feature index of the battery feature per slot, so that the listener recognizes battery events
// Key: USB PID of the receiver and device index. Thread-safe.
namespace hidpp
{
    class LinkRegistry
    {
    public:
        void SetLink(uint16_t productId, uint8_t slot, bool connected)
        {
            std::lock_guard lock(mutex_);
            links_[{ productId, slot }] = connected;
        }

        // nullopt: no notification from the receiver for this slot since startup.
        std::optional<bool> GetLink(uint16_t productId, uint8_t slot) const
        {
            std::lock_guard lock(mutex_);
            auto it = links_.find({ productId, slot });
            return it == links_.end() ? std::nullopt : std::optional<bool>(it->second);
        }

        void SetBatteryIndex(uint16_t productId, uint8_t slot, uint8_t featureIndex)
        {
            std::lock_guard lock(mutex_);
            batteryIndex_[{ productId, slot }] = featureIndex;
        }

        std::optional<uint8_t> GetBatteryIndex(uint16_t productId, uint8_t slot) const
        {
            std::lock_guard lock(mutex_);
            auto it = batteryIndex_.find({ productId, slot });
            return it == batteryIndex_.end() ? std::nullopt : std::optional<uint8_t>(it->second);
        }

    private:
        using Key = std::pair<uint16_t, uint8_t>;
        mutable std::mutex mutex_;
        std::map<Key, bool> links_;
        std::map<Key, uint8_t> batteryIndex_;
    };
}
