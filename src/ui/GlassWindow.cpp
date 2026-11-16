#include "GlassWindow.h"

#include <QWidget>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

struct AccentPolicy
{
    int accentState;
    int accentFlags;
    unsigned int gradientColor;
    int animationId;
};

struct WindowCompositionAttribData
{
    int attribute;
    void* data;
    SIZE_T dataSize;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WindowCompositionAttribData*);
using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);

constexpr int ACCENT_DISABLED = 0;
constexpr int ACCENT_ENABLE_ACRYLICBLURBEHIND = 4;
constexpr int WCA_ACCENT_POLICY = 19;

constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
constexpr DWORD DWMWA_BORDER_COLOR = 34;

constexpr int DWMWCP_DONOTROUND = 1;
constexpr int DWMWCP_ROUND = 2;

} // namespace

bool GlassWindow::apply(QWidget* widget, bool glass, const QColor& tint, int tintAlpha)
{
    if (!widget) {
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd) {
        return false;
    }

    static HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
    static HMODULE user32 = GetModuleHandleW(L"user32.dll");

    const auto dwmSet = dwmapi
        ? reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwmapi, "DwmSetWindowAttribute"))
        : nullptr;
    const auto setWca = user32
        ? reinterpret_cast<SetWindowCompositionAttributeFn>(
              GetProcAddress(user32, "SetWindowCompositionAttribute"))
        : nullptr;

    if (dwmSet) {
        const BOOL dark = TRUE;
        dwmSet(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

        const int corner = glass ? DWMWCP_ROUND : DWMWCP_DONOTROUND;
        dwmSet(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

        const DWORD noBorder = 0xFFFFFFFE; // DWMWA_COLOR_NONE
        dwmSet(hwnd, DWMWA_BORDER_COLOR, &noBorder, sizeof(noBorder));
    }

    if (!setWca) {
        return false;
    }

    AccentPolicy policy{};
    policy.accentState = glass ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_DISABLED;
    policy.accentFlags = 0;
    // ABGR
    policy.gradientColor =
        (static_cast<unsigned int>(qBound(0, tintAlpha, 255)) << 24) |
        (static_cast<unsigned int>(tint.blue()) << 16) |
        (static_cast<unsigned int>(tint.green()) << 8) |
        static_cast<unsigned int>(tint.red());

    WindowCompositionAttribData data{};
    data.attribute = WCA_ACCENT_POLICY;
    data.data = &policy;
    data.dataSize = sizeof(policy);

    return setWca(hwnd, &data) != FALSE && glass;
}

#else

bool GlassWindow::apply(QWidget*, bool, const QColor&, int)
{
    return false;
}

#endif
