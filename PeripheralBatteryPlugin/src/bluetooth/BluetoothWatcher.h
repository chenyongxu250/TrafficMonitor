#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

// Reports connection changes of paired Bluetooth devices immediately (Windows.Devices.Enumeration.
// DeviceWatcher, AssociationEndpoint, property System.Devices.Aep.IsConnected).
// Call Start() on the MTA worker; the events come from the thread pool.
// Lifetime until the process exits (see HidppListener).
class BluetoothWatcher
{
public:
    using Trigger = std::function<void(std::wstring reason)>;

    explicit BluetoothWatcher(Trigger trigger);
    ~BluetoothWatcher();

    BluetoothWatcher(const BluetoothWatcher&) = delete;
    BluetoothWatcher& operator=(const BluetoothWatcher&) = delete;

    bool Start();

    // If the watcher is not running (anymore) – Start failed, Stopped or Aborted (after which, per the
    // docs, no more events arrive) –, it is restarted. Returns: nullopt = was already running,
    // otherwise whether the restart succeeded. Call from the worker.
    std::optional<bool> EnsureRunning();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
