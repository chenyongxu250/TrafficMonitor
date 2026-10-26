#include "hidpp/HidDevice.h"

#include <hidsdi.h>
#include <setupapi.h>

#include <utility>

namespace
{
    struct HandleCloser
    {
        HANDLE handle;
        ~HandleCloser()
        {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
    };

    std::wstring ProductString(HANDLE handle)
    {
        wchar_t buffer[128] = {};
        if (!HidD_GetProductString(handle, buffer, sizeof(buffer)))
            return {};
        return buffer;
    }

    // Read the path and properties of a collection; access 0 is enough for attributes and caps.
    bool Describe(const std::wstring& path, uint16_t vendorId, HidDeviceInfo& info)
    {
        HandleCloser file{ CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_EXISTING, 0, nullptr) };
        if (file.handle == INVALID_HANDLE_VALUE)
            return false;

        HIDD_ATTRIBUTES attributes{};
        attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(file.handle, &attributes))
            return false;
        // Skip other vendors' devices right away: the product name below is a request to the device
        // and could wake a sleeping USB device.
        if (attributes.VendorID != vendorId)
            return false;

        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!HidD_GetPreparsedData(file.handle, &preparsed))
            return false;
        HIDP_CAPS caps{};
        const NTSTATUS status = HidP_GetCaps(preparsed, &caps);
        HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS)
            return false;

        info.path = path;
        info.vendorId = attributes.VendorID;
        info.productId = attributes.ProductID;
        info.usagePage = caps.UsagePage;
        info.usage = caps.Usage;
        info.inputReportLength = caps.InputReportByteLength;
        info.outputReportLength = caps.OutputReportByteLength;
        info.product = ProductString(file.handle);
        return true;
    }
}

std::vector<HidDeviceInfo> EnumerateHidDevices(uint16_t vendorId)
{
    std::vector<HidDeviceInfo> result;

    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO set = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE)
        return result;

    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &hidGuid, i, &interfaceData); ++i)
    {
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, nullptr, 0, &required, nullptr);
        if (required == 0)
            continue;

        std::vector<BYTE> buffer(required);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &interfaceData, detail, required, nullptr, nullptr))
            continue;

        HidDeviceInfo info;
        if (Describe(detail->DevicePath, vendorId, info))
            result.push_back(std::move(info));
    }

    SetupDiDestroyDeviceInfoList(set);
    return result;
}

HidDevice::~HidDevice()
{
    Close();
}

HidDevice::HidDevice(HidDevice&& other) noexcept
    : handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE))
    , event_(std::exchange(other.event_, nullptr))
{
}

HidDevice& HidDevice::operator=(HidDevice&& other) noexcept
{
    if (this != &other)
    {
        Close();
        handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
        event_ = std::exchange(other.event_, nullptr);
    }
    return *this;
}

bool HidDevice::Open(const std::wstring& path)
{
    Close();
    handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE)
        return false;
    event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (event_ == nullptr)
    {
        Close();
        return false;
    }
    return true;
}

bool HidDevice::IsOpen() const
{
    return handle_ != INVALID_HANDLE_VALUE;
}

void HidDevice::Close()
{
    if (handle_ != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(handle_, nullptr);
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    if (event_ != nullptr)
    {
        CloseHandle(event_);
        event_ = nullptr;
    }
}

bool HidDevice::Write(const uint8_t* data, size_t length, DWORD timeoutMs)
{
    if (!IsOpen())
        return false;

    OVERLAPPED overlapped{};
    overlapped.hEvent = event_;
    ResetEvent(event_);

    DWORD written = 0;
    if (!WriteFile(handle_, data, static_cast<DWORD>(length), nullptr, &overlapped)
        && GetLastError() != ERROR_IO_PENDING)
    {
        return false;
    }
    if (WaitForSingleObject(event_, timeoutMs) != WAIT_OBJECT_0)
    {
        CancelIoEx(handle_, &overlapped);
        GetOverlappedResult(handle_, &overlapped, &written, TRUE);
        return false;
    }
    return GetOverlappedResult(handle_, &overlapped, &written, FALSE) && written == length;
}

int HidDevice::Read(uint8_t* buffer, size_t length, DWORD timeoutMs)
{
    if (!IsOpen())
        return -1;

    OVERLAPPED overlapped{};
    overlapped.hEvent = event_;
    ResetEvent(event_);

    DWORD read = 0;
    if (!ReadFile(handle_, buffer, static_cast<DWORD>(length), nullptr, &overlapped)
        && GetLastError() != ERROR_IO_PENDING)
    {
        return -1;
    }
    if (WaitForSingleObject(event_, timeoutMs) != WAIT_OBJECT_0)
    {
        CancelIoEx(handle_, &overlapped);
        // The read may still have completed between the timeout and the cancellation.
        if (GetOverlappedResult(handle_, &overlapped, &read, TRUE) && read > 0)
            return static_cast<int>(read);
        return 0;
    }
    if (!GetOverlappedResult(handle_, &overlapped, &read, FALSE))
        return -1;
    return static_cast<int>(read);
}
