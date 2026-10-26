#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

// Thin wrapper around the Win32 HID API (SetupAPI + hid.dll), without a third-party library.

struct HidDeviceInfo
{
    std::wstring path;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint16_t usagePage = 0;
    uint16_t usage = 0;
    uint16_t inputReportLength = 0;
    uint16_t outputReportLength = 0;
    std::wstring product;
};

// All present HID collections of one vendor. On Windows, every top-level collection is a
// separate device with its own path.
std::vector<HidDeviceInfo> EnumerateHidDevices(uint16_t vendorId);

class HidDevice
{
public:
    HidDevice() = default;
    ~HidDevice();

    HidDevice(HidDevice&& other) noexcept;
    HidDevice& operator=(HidDevice&& other) noexcept;
    HidDevice(const HidDevice&) = delete;
    HidDevice& operator=(const HidDevice&) = delete;

    // Opens for reading and writing, shared with other programs (e.g. G Hub).
    bool Open(const std::wstring& path);
    bool IsOpen() const;
    void Close();

    // Writes a complete output report (first byte = report ID).
    bool Write(const uint8_t* data, size_t length, DWORD timeoutMs);

    // Reads an input report (first byte = report ID). Returns the number of bytes,
    // 0 on timeout, -1 on error (e.g. device removed).
    int Read(uint8_t* buffer, size_t length, DWORD timeoutMs);

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE event_ = nullptr;
};
