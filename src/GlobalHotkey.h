#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    static GlobalHotkey& instance();

    bool registerHotkey(int key = 'P', bool ctrl = true, bool alt = true);
    void unregisterHotkey();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void hotkeyTriggered();

private:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;

    bool m_registered = false;
    static constexpr int HotkeyId = 0xDEAE;
};
