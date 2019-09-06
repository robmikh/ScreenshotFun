#include "pch.h"
#include "DummyWindow.h"

void DummyWindow::RegisterWindowClass()
{
    auto instance = GetModuleHandleW(nullptr);
    winrt::check_bool(instance);

    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIconW(instance, IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"DummyWindow";
    wcex.hIconSm = LoadIconW(wcex.hInstance, IDI_APPLICATION);
    winrt::check_bool(RegisterClassExW(&wcex));
}

DummyWindow::DummyWindow()
{
    auto instance = GetModuleHandleW(nullptr);
    winrt::check_bool(instance);

    // Create our close handle
    m_windowClosed = wil::shared_event(wil::EventOptions::None);

    winrt::check_bool(CreateWindowW(L"DummyWindow", L"DummyWindow", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 400, nullptr, nullptr, instance, this));
    WINRT_ASSERT(m_window);

    ShowWindow(m_window, SW_SHOWDEFAULT);
    UpdateWindow(m_window);
}

LRESULT DummyWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    if (WM_DESTROY == message)
    {
        m_windowClosed.SetEvent();
        return 0;
    }

    switch (message)
    {
    case WM_KEYUP:
        if (wparam == VK_ESCAPE)
        {
            m_windowClosed.SetEvent();
            return 0;
        }
        break;
    }

    return base_type::MessageHandler(message, wparam, lparam);
}