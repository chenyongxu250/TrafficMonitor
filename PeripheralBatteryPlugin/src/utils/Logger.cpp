#include "utils/Logger.h"

#include <windows.h>

#include <mutex>

namespace
{
    constexpr LONGLONG kMaxSize = 1024 * 1024;

    std::mutex logMutex;
    std::wstring logPath;
    bool logVerbose = false;

    std::string ToUtf8(const std::wstring& text)
    {
        if (text.empty())
            return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
            nullptr, nullptr);
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr,
            nullptr);
        return result;
    }

    void RotateIfLarge()
    {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW(logPath.c_str(), GetFileExInfoStandard, &data))
            return;
        const LONGLONG size = (static_cast<LONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        if (size > kMaxSize)
            MoveFileExW(logPath.c_str(), (logPath + L".1").c_str(), MOVEFILE_REPLACE_EXISTING);
    }

    void Write(const wchar_t* level, const std::wstring& message)
    {
        std::lock_guard lock(logMutex);
        if (logPath.empty())
            return;

        RotateIfLarge();

        SYSTEMTIME t{};
        GetLocalTime(&t);
        wchar_t stamp[40];
        swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        const std::string line = ToUtf8(std::wstring(stamp) + level + L" " + message) + "\r\n";

        HANDLE file = CreateFileW(logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return;
        DWORD written = 0;
        WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        CloseHandle(file);
    }
}

namespace logging
{
    void Configure(const std::wstring& newPath, bool newVerbose)
    {
        std::lock_guard lock(logMutex);
        logPath = newPath;
        logVerbose = newVerbose;
    }

    bool IsVerbose()
    {
        std::lock_guard lock(logMutex);
        return logVerbose && !logPath.empty();
    }

    void Error(const std::wstring& message)
    {
        Write(L"ERROR", message);
    }

    void Info(const std::wstring& message)
    {
        Write(L"INFO ", message);
    }

    void Verbose(const std::wstring& message)
    {
        if (IsVerbose())
            Write(L"DEBUG", message);
    }
}
