#pragma once
#include "DesktopWindow.h"

struct DummyWindow : DesktopWindow<DummyWindow>
{
    static void RegisterWindowClass();
    DummyWindow();
    LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);

    wil::shared_event Closed() { return m_windowClosed; }

private:
    wil::shared_event m_windowClosed{ nullptr };
};