#pragma once

#include <optional>
#include <string>
#include <vector>

// Battery level of Bluetooth devices, as Windows itself knows it.
//
// - Paired devices via Windows.Devices.Enumeration (AssociationEndpoint); connection status
//   and container ID via the documented AEP properties.
// - Battery via the device property {104EA319-6EE2-4701-BD47-8DDBF425BBE5} 2 (byte, percent)
//   on a node of the same container. Not documented by Microsoft; verified on the MOMENTUM 4 at
//   the node "Hands-Free AG". For a disconnected device the last known value stays there –
//   always check `connected` before the battery.

struct BluetoothNode
{
    std::wstring name;
    std::wstring id;
    std::optional<int> battery;
    std::wstring batteryType;   // Type of the battery property, if present
};

struct BluetoothDeviceReport
{
    std::wstring name;
    std::wstring transport;     // "classic" or "le"
    std::optional<bool> connected;
    std::wstring address;
    std::wstring containerId;
    std::optional<int> battery;
    std::wstring batterySource;
    std::vector<BluetoothNode> nodes;
    std::wstring error;
};

struct BluetoothScanResult
{
    std::vector<BluetoothDeviceReport> devices;
    std::wstring error;         // Empty if the device list could be queried
};

// Blocks (~80 ms on the test machine). Only call from an MTA thread, never from TrafficMonitor's
// UI or monitor thread.
BluetoothScanResult ScanBluetoothDevices();
