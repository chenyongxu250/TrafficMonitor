#pragma once

#include <windows.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <string>

class ITrafficMonitor;

// Shows notifications via ITrafficMonitor::ShowNotifyMessage.
//
// ShowNotifyMessage is not thread-safe in TrafficMonitor 1.86 (it changes the main window's tray
// icon data and calls SetTimer on its window) and must only run on the UI thread.
// That is why Initialize() creates an invisible message-only window on the UI thread; Post() enqueues
// from any thread and wakes the window, which then shows the message on the UI thread.
class UiNotifier
{
public:
    UiNotifier() = default;
    UiNotifier(const UiNotifier&) = delete;
    UiNotifier& operator=(const UiNotifier&) = delete;

    // Call on the UI thread (ITMPlugin::OnInitialize). Holds a reference to the DLL so that the
    // window procedure is never called from unloaded code.
    bool Initialize(ITrafficMonitor* app);

    // Thread-safe. Without Initialize(), nothing is shown.
    void Post(std::wstring message);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void Flush();

    ITrafficMonitor* app_ = nullptr;
    std::atomic<HWND> window_{ nullptr };   // set on the UI thread, read by the worker
    std::mutex mutex_;
    std::deque<std::wstring> queue_;
};
