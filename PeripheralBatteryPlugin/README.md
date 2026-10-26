# Peripheral Battery Plugin for TrafficMonitor

A 64-bit TrafficMonitor plugin that provides three separately selectable display items:

1. **Mouse Battery** — Logitech wireless mouse through Logitech HID++ (based on the existing PeripheralBatteryPlugin provider).
2. **Headphones Battery** — Windows Bluetooth battery information, when exposed by the device/driver.
3. **Keyboard Battery** — best-effort Windows Bluetooth battery property lookup. Keyboard support is optional; if Windows does not report a percentage, the item shows an unknown/disconnected indicator rather than inventing a value.

## Build without installing Visual Studio locally

The repository's GitHub Actions workflow builds this project on a Windows Server 2022 runner using MSBuild and the v143 toolset. Every branch push that changes this directory triggers a Release x64 build and publishes a downloadable `PeripheralBatteryPlugin-x64` artifact.

Workflow: `.github/workflows/build-peripheral-battery.yml`

## Install

1. Exit TrafficMonitor.
2. Copy `PeripheralBatteryPlugin.dll` directly into `plugins` beside `TrafficMonitor.exe` (do not create a subfolder).
3. Start TrafficMonitor and open **Display Settings**.
4. Select any combination of **Headphones Battery**, **Mouse Battery**, and **Keyboard Battery**.
5. If the keyboard does not appear to report a value, edit `PeripheralBattery.ini` in the plugin configuration folder. Set `[Keyboard] NameContains` to a distinctive part of the Windows device name.

## Notes

- Windows does not expose battery percentages for every Bluetooth device. Headphones and keyboard values depend on the driver and Windows device properties.
- Keyboard support is a best-effort feature; it does not read keyboard keys.
- The plugin reads Logitech battery information through HID++ and uses a background worker, so the display getters do not perform synchronous device queries.
- The cloud build proves that the source compiles and links as x64. It does not replace physical-device testing on the user's mouse, headset, and keyboard.
- This plugin incorporates source components from [Ariazonaa/PeripheralBatteryPlugin](https://github.com/Ariazonaa/PeripheralBatteryPlugin). See the upstream repository for its license and provider documentation.
