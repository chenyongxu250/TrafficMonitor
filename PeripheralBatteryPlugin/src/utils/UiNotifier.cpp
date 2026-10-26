#include "utils/UiNotifier.h"

#include <PluginInterface.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    constexpr wchar_t kWindowClass[] = L"PeripheralBatteryPluginNotifier";
    constexpr UINT kFlushMessage = WM_APP + 1;
}

bool UiNotifier::Initialize(ITrafficMonitor* app)
{
    if (app == nullptr || window_ != nullptr)
        return false;

    const HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &UiNotifier::WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&__ImageBase), &self))
        return false;

    app_ = app;
    HWND window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this);
    if (window == nullptr)
    {
        app_ = nullptr;
        FreeLibrary(self);
        return false;
    }
    window_ = window;
    return true;
}

void UiNotifier::Post(std::wstring message)
{
    {
        std::lock_guard lock(mutex_);
        queue_.push_back(std::move(message));
    }
    if (HWND window = window_.load())
        PostMessageW(window, kFlushMessage, 0, 0);
}

void UiNotifier::Flush()
{
    std::deque<std::wstring> pending;
    {
        std::lock_guard lock(mutex_);
        pending.swap(queue_);
    }
    for (const std::wstring& message : pending)
    {
        try
        {
            app_->ShowNotifyMessage(message.c_str());
        }
        catch (...)
        {
        }
    }
}

LRESULT CALLBACK UiNotifier::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    else if (message == kFlushMessage)
    {
        if (auto* self = reinterpret_cast<UiNotifier*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)))
            self->Flush();
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
