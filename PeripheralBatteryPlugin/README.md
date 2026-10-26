# Peripheral Battery Plugin (initial source drop)

A TrafficMonitor x64 DLL exposing three independent display items:
- INPHIC mouse battery
- Bluetooth headphones battery
- Wireless keyboard battery

## Build
Open `PeripheralBatteryPlugin.vcxproj` in Visual Studio 2022 with the Desktop development with C++ workload. Select Release / x64 and build. Copy `bin\\x64\\Release\\PeripheralBatteryPlugin.dll` into the `plugins` directory beside `TrafficMonitor.exe`.

## Current limitations
- INPHIC currently accepts the documented 5-byte IN10 push frame: `03 <type> 40 <device-number> <percent>`. IN9 Feature Report polling is not implemented yet.
- Headphones and keyboard use Windows PnP battery properties (System.Devices.BatteryLife and a known Bluetooth Hands-Free property). Windows only publishes a percentage for some devices/drivers; otherwise the item displays `—`.
- This source has not been compiled or tested against physical devices in this environment.
- The first implementation uses synchronous HID ReadFile for the matched vendor interfaces. If testing shows that it blocks on a device that does not push a report promptly, the INPHIC reader must be converted to per-interface overlapped I/O before release.
- The code does not parse keypresses or mouse movement. It only accepts a matching INPHIC status frame.

The project uses the TrafficMonitor interface header in `../include/PluginInterface.h` and exports `TMPluginGetInstance`.
