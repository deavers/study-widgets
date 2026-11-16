#include "Theme.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QStyle>
#include <QWidget>

#include <cmath>

namespace Theme {

namespace {

Accent g_accent = Accent::Violet;
GlassMode g_glass = GlassMode::Glass;
bool g_loaded = false;

void ensureLoaded()
{
    if (g_loaded) {
        return;
    }
    g_loaded = true;
    QSettings settings;
    g_accent = static_cast<Accent>(qBound(0, settings.value("theme/accent", 2).toInt(), 2));
    g_glass = static_cast<GlassMode>(qBound(0, settings.value("theme/glass", 0).toInt(), 1));
}

int px(double scale, int base)
{
    return qMax(8, static_cast<int>(std::round(base * scale)));
}

} // namespace

Notifier& notifier()
{
    static Notifier instance;
    return instance;
}

Accent accent()
{
    ensureLoaded();
    return g_accent;
}

void setAccent(Accent value)
{
    ensureLoaded();
    if (g_accent == value) {
        return;
    }
    g_accent = value;
    QSettings settings;
    settings.setValue("theme/accent", static_cast<int>(value));
    notifier().emitChanged();
}

GlassMode glassMode()
{
    ensureLoaded();
    return g_glass;
}

void setGlassMode(GlassMode value)
{
    ensureLoaded();
    if (g_glass == value) {
        return;
    }
    g_glass = value;
    QSettings settings;
    settings.setValue("theme/glass", static_cast<int>(value));
    notifier().emitChanged();
}

QColor accentColor()
{
    switch (accent()) {
    case Accent::Green:
        return QColor(0xA3, 0xD9, 0xB1);
    case Accent::Violet:
        return QColor(0xC4, 0xB5, 0xFD);
    case Accent::Cyan:
    default:
        return QColor(0x8F, 0xD3, 0xC8);
    }
}

QString accentHex()
{
    return accentColor().name(QColor::HexRgb);
}

QString rgba(const QColor& color, int alpha)
{
    return QString("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(alpha);
}

QColor windowTint()
{
    return QColor(0x0E, 0x10, 0x20);
}

QString uiFamily() { return QStringLiteral("Segoe UI Variable Text"); }
QString displayFamily() { return QStringLiteral("Segoe UI Variable Display"); }
QString monoFamily() { return QStringLiteral("Cascadia Mono"); }
QString iconFamily() { return QStringLiteral("Segoe Fluent Icons"); }

QFont monoFont(int pixelSize, int weight)
{
    QFont font(monoFamily());
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

QFont uiFont(int pixelSize, int weight)
{
    QFont font(uiFamily());
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

QString glyph(int codePoint)
{
    return QString(QChar(static_cast<char16_t>(codePoint)));
}

QIcon icon(int codePoint, const QColor& color, int logicalPx)
{
    const qreal dpr = 2.0;
    QPixmap pixmap(static_cast<int>(logicalPx * dpr), static_cast<int>(logicalPx * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font(iconFamily());
    font.setPixelSize(logicalPx - 2);
    painter.setFont(font);
    painter.setPen(color.isValid() ? color : QColor(0xF2, 0xF5, 0xFA));
    painter.drawText(QRect(0, 0, logicalPx, logicalPx), Qt::AlignCenter, glyph(codePoint));
    painter.end();

    return QIcon(pixmap);
}

void setState(QWidget* widget, const char* name, const QString& value)
{
    if (!widget) {
        return;
    }
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QString styleSheet(double scale)
{
    const QColor acc = accentColor();
    const QString A = acc.name(QColor::HexRgb);
    const QString accSoft = rgba(acc, 34);
    const QString accLine = rgba(acc, 120);
    const QString accHover = acc.lighter(118).name(QColor::HexRgb);

    const QString ui = "\"" + uiFamily() + "\", \"Segoe UI\", sans-serif";
    const QString mono = "\"" + monoFamily() + "\", \"Consolas\", monospace";
    const QString icon = "\"" + iconFamily() + "\"";

    const QString text = "#EEF0F8";
    const QString dim = "#A9AFC9";
    const QString faint = "#7B819F";
    const QString ok = "#8FD6A8";
    const QString warn = "#F2C47E";
    const QString danger = "#F29AA8";

    const QString surface1 = "rgba(255,255,255,13)";
    const QString surface2 = "rgba(255,255,255,22)";
    const QString stroke = "rgba(255,255,255,26)";
    const QString strokeStrong = "rgba(255,255,255,56)";

    QString css;
    css += QString(R"(
QWidget { font-family: %1; color: %2; }
QLabel { background: transparent; font-size: %3px; }
QLabel[role="title"]   { font-family: %4; font-size: %5px; font-weight: 600; }
QLabel[role="dim"]     { color: %6; font-size: %7px; }
QLabel[role="faint"]   { color: %8; font-size: %7px; }
QLabel[role="section"] { color: %8; font-family: %9; font-size: %10px; font-weight: 600; }
QLabel[role="mono"]    { font-family: %9; font-size: %3px; font-weight: 600; }
QLabel[role="monoDim"] { font-family: %9; color: %6; font-size: %7px; }
QLabel[role="big"]     { font-family: %9; font-size: %11px; font-weight: 600; color: %2; }
QLabel[role="bigAccent"] { font-family: %9; font-size: %11px; font-weight: 600; color: %12; }
QLabel[role="medium"]  { font-family: %9; font-size: %13px; font-weight: 600; color: %12; }
QLabel[role="accent"]  { color: %12; font-weight: 600; }
QLabel[role="ok"]      { color: %14; font-weight: 600; }
QLabel[role="warn"]    { color: %15; font-weight: 600; }
QLabel[role="danger"]  { color: %16; font-weight: 600; }
QLabel[role="prompt"]  { font-family: %9; color: %6; font-size: %7px; }
QLabel[role="arabic"]  { font-family: "Amiri", "Segoe UI"; font-size: %17px; font-weight: 700; color: %12; }
QLabel[role="badge"]   { font-family: %9; font-size: %7px; font-weight: 600; color: %12;
                         background: %18; border: 1px solid %19; border-radius: 8px; padding: 2px 9px; }
QLabel[role="badge"][state="alt"] { color: #C7B6FF; background: rgba(167,139,250,36); border: 1px solid rgba(167,139,250,120); }
QLabel[role="note"]    { background: %20; border: 1px solid %21; border-radius: 10px; padding: 7px 10px;
                         color: #D5DDEB; font-size: %7px; }
QLabel[role="banner"]  { background: %18; border: 1px solid %19; border-radius: 10px; padding: 7px 10px;
                         color: %2; font-size: %7px; }
QLabel[role="banner"][state="ok"]     { background: rgba(143,214,168,30); border: 1px solid rgba(143,214,168,120); color: #B8F5D2; }
QLabel[role="banner"][state="danger"] { background: rgba(242,154,168,30); border: 1px solid rgba(242,154,168,130); color: #FFC9D3; }
QLabel[role="banner"][state="warn"]   { background: rgba(242,196,126,30); border: 1px solid rgba(242,196,126,130); color: #FFE2B0; }
QLabel[role="pill"]    { font-family: %9; font-size: %10px; font-weight: 600; color: %6;
                         background: %20; border: 1px solid %21; border-radius: 7px; padding: 1px 7px; }
QLabel[role="divider"] { background: %21; max-height: 1px; min-height: 1px; }

QWidget[role="card"] { background: %20; border: 1px solid %21; border-radius: 16px; }
QWidget[role="card"][state="accent"] { background: %18; border: 1px solid %19; }
QWidget[role="card"][state="done"]   { background: rgba(143,214,168,22); border: 1px solid rgba(143,214,168,90); }
QWidget[role="card"][state="due"]    { background: rgba(242,154,168,24); border: 1px solid rgba(242,154,168,120); }
QWidget[role="card"] QLabel { background: transparent; border: none; }
QWidget[role="card"] QCheckBox { background: transparent; border: none; }

QPushButton { background: %20; border: 1px solid %21; border-radius: 10px; padding: 6px 12px;
              color: %2; font-size: %3px; font-weight: 600; }
QPushButton:hover { background: %22; border-color: %23; }
QPushButton:pressed { background: rgba(255,255,255,8); }
QPushButton:checked { background: %18; border: 1px solid %19; color: %12; }
QPushButton:disabled { background: rgba(255,255,255,6); border-color: rgba(255,255,255,12); color: #4C566A; }
QPushButton[variant="primary"] { background: %12; color: #14142B; border: none; font-weight: 700; }
QPushButton[variant="primary"]:hover { background: %24; }
QPushButton[variant="primary"]:disabled { background: rgba(255,255,255,12); color: #4C566A; }
QPushButton[variant="danger"] { background: rgba(242,154,168,28); border: 1px solid rgba(242,154,168,110); color: #FF9AAE; }
QPushButton[variant="danger"]:hover { background: rgba(242,154,168,52); border-color: %16; }
QPushButton[variant="ghost"]  { background: transparent; border: none; color: %6; padding: 3px 7px; }
QPushButton[variant="ghost"]:hover { background: %20; color: %2; }
QPushButton[variant="mini"] { padding: 2px 8px; border-radius: 7px; font-size: %7px; }
QPushButton[variant="chip"] { background: %20; border: 1px solid %21; border-radius: 9px; padding: 4px 10px;
                              font-family: %9; font-size: %7px; font-weight: 600; }
QPushButton[variant="chip"]:checked { background: %18; border: 1px solid %19; color: %12; }
QPushButton[variant="icon"] { font-family: %25; font-size: %3px; padding: 0px; }

QLineEdit, QSpinBox, QComboBox { background: rgba(0,0,0,70); border: 1px solid %21; border-radius: 9px;
              padding: 5px 9px; color: %2; font-size: %3px; selection-background-color: %19; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: %12; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: #161933; border: 1px solid %19; color: %2; padding: 4px;
              selection-background-color: %18; selection-color: %12; font-size: %3px; outline: none; }

QProgressBar { background: rgba(255,255,255,16); border: none; border-radius: 4px; text-align: center;
               color: %2; font-family: %9; font-size: %10px; font-weight: 600; min-height: %26px; max-height: %26px; }
QProgressBar::chunk { background: %12; border-radius: 4px; }
QProgressBar[tier="A"]::chunk { background: #FF5C7A; }
QProgressBar[tier="B"]::chunk { background: #FFB547; }
QProgressBar[tier="C"]::chunk { background: #3DDC84; }

QScrollArea { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 6px; margin: 2px 0; }
QScrollBar::handle:vertical { background: rgba(255,255,255,46); min-height: 24px; border-radius: 3px; }
QScrollBar::handle:vertical:hover { background: %12; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QScrollBar:horizontal { height: 0px; }

QCheckBox { spacing: 8px; color: %2; font-size: %3px; background: transparent; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1.5px solid rgba(255,255,255,70);
                       background: rgba(0,0,0,60); }
QCheckBox::indicator:hover { border-color: %12; }
QCheckBox::indicator:checked { background: %12; border-color: %12; }

QToolTip { background: #161933; color: %2; border: 1px solid %19; padding: 4px 8px; border-radius: 6px; }
)")
               .arg(ui)                         // 1
               .arg(text)                       // 2
               .arg(px(scale, 12))              // 3
               .arg(displayFamily().prepend('"').append('"')) // 4
               .arg(px(scale, 15))              // 5
               .arg(dim)                        // 6
               .arg(px(scale, 11))              // 7
               .arg(faint)                      // 8
               .arg(mono)                       // 9
               .arg(px(scale, 10))              // 10
               .arg(px(scale, 44))              // 11
               .arg(A)                          // 12
               .arg(px(scale, 20))              // 13
               .arg(ok)                         // 14
               .arg(warn)                       // 15
               .arg(danger)                     // 16
               .arg(px(scale, 20))              // 17
               .arg(accSoft)                    // 18
               .arg(accLine)                    // 19
               .arg(surface1)                   // 20
               .arg(stroke)                     // 21
               .arg(surface2)                   // 22
               .arg(strokeStrong)               // 23
               .arg(accHover)                   // 24
               .arg(icon)                       // 25
               .arg(px(scale, 8));              // 26
    return css;
}

} // namespace Theme
