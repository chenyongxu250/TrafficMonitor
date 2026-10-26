#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "config/Config.h"

// Settings window (plan §34/§35). Knows neither hardware nor the INI file: suggestions and saving
// come from the caller as functions.
struct OptionsDialogContext
{
    Config config;                      // initial values (from the INI)
    std::wstring headphonesDetected;    // last detected devices, as a suggestion in the "Device" field
    std::wstring mouseDetected;

    // Names of paired Bluetooth devices. Blocking – runs on its own MTA thread,
    // the result is added to the suggestion list afterwards.
    std::function<std::vector<std::wstring>()> scanBluetoothNames;

    // Saves the new values. On false the window stays open and shows `error`.
    std::function<bool(const Config& config, std::wstring& error)> save;
};

// Modal; UI thread only. true = saved with OK and something changed.
bool RunOptionsDialog(HWND owner, const OptionsDialogContext& context);

// Brings an already open settings window to the front (UI thread). false = none open.
bool BringOptionsDialogToFront();
