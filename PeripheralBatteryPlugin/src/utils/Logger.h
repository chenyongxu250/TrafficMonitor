#pragma once

#include <string>

// Log file PeripheralBatteryPlugin.log next to the INI (Plan §36).
// Default: only errors and a few important events; VerboseLogging=1 writes every cycle.
// Thread-safe. As long as no path is set, nothing is written. If the file grows larger than
// 1 MB, it is moved to .log.1 (one previous file is kept).
namespace logging
{
    void Configure(const std::wstring& path, bool verbose);
    bool IsVerbose();

    void Error(const std::wstring& message);
    void Info(const std::wstring& message);
    void Verbose(const std::wstring& message);
}
