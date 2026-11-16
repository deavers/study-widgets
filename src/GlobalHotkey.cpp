#include "GlobalHotkey.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

GlobalHotkey& GlobalHotkey::instance()
{
    static GlobalHotkey hotkey;
    return hotkey;
}

GlobalHotkey::GlobalHotkey(QObject* parent)
    : QObject(parent)
{
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterHotkey();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool GlobalHotkey::registerHotkey(int key, bool ctrl, bool alt)
{
#ifdef Q_OS_WIN
    unregisterHotkey();

    UINT fsModifiers = 0;
    if (ctrl) fsModifiers |= MOD_CONTROL;
    if (alt) fsModifiers |= MOD_ALT;
    fsModifiers |= 0x4000; // MOD_NOREPEAT

    BOOL success = RegisterHotKey(nullptr, HotkeyId, fsModifiers, key);
    m_registered = (success != FALSE);
    return m_registered;
#else
    return false;
#endif
}

void GlobalHotkey::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered) {
        UnregisterHotKey(nullptr, HotkeyId);
        m_registered = false;
    }
#endif
}

bool GlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == HotkeyId) {
            emit hotkeyTriggered();
            return true;
        }
    }
#endif
    return false;
}
