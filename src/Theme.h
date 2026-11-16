#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QString>

class QWidget;

// "Deave Glass" design system: one place for colors, fonts, radii and the
// shared stylesheet. Widgets never hard-code hex colors; they set a `role`,
// `variant` or `state` property and the stylesheet does the rest.
namespace Theme {

enum class Accent { Cyan = 0, Green = 1, Violet = 2 };
enum class GlassMode { Glass = 0, Soft = 1 };

class Notifier : public QObject
{
    Q_OBJECT
public:
    void emitChanged() { emit changed(); }
signals:
    void changed();
};

Notifier& notifier();

Accent accent();
void setAccent(Accent value);

GlassMode glassMode();
void setGlassMode(GlassMode value);

QColor accentColor();
QString accentHex();
QString rgba(const QColor& color, int alpha);

// Surface color for the window background (tinted, alpha applied by caller).
QColor windowTint();

QString uiFamily();
QString displayFamily();
QString monoFamily();
QString iconFamily();

QFont monoFont(int pixelSize, int weight = QFont::DemiBold);
QFont uiFont(int pixelSize, int weight = QFont::Normal);

// Fluent glyph rendered as a crisp QIcon (like SF Symbols).
QIcon icon(int codePoint, const QColor& color = QColor(), int logicalPx = 16);
QString glyph(int codePoint);

// Shared application stylesheet, scaled by `scale` (0.65..1.75).
QString styleSheet(double scale = 1.0);

// Sets a dynamic property and re-polishes so [state="..."] selectors apply.
void setState(QWidget* widget, const char* name, const QString& value);

inline QColor surface1() { return QColor(20, 22, 40, 205); }
inline QColor surface2() { return QColor(28, 31, 54, 220); }
inline QColor surface3() { return QColor(38, 42, 70, 240); }
inline QColor stroke() { return QColor(255, 255, 255, 20); }
inline QColor strokeStrong() { return QColor(255, 255, 255, 45); }
inline QColor textColor() { return QColor(238, 240, 248); }
inline QColor textDim() { return QColor(169, 175, 201); }
inline QColor textFaint() { return QColor(123, 129, 159); }
inline QColor statusOk() { return QColor(143, 214, 168); }
inline QColor statusWarn() { return QColor(242, 196, 126); }
inline QColor statusDanger() { return QColor(242, 154, 168); }
inline QColor accentTint(int alpha = 40) {
    QColor c = accentColor();
    c.setAlpha(alpha);
    return c;
}
inline int radiusWindow() { return 14; }
inline int radiusCard() { return 14; }
inline int radiusTile() { return 10; }
inline QFont fontMono(int pixelSize, int weight = QFont::Normal) { return monoFont(pixelSize, weight); }
inline QFont fontUi(int pixelSize, int weight = QFont::Normal) { return uiFont(pixelSize, weight); }
inline QFont fontIcon(int pixelSize) {
    QFont f(iconFamily());
    f.setPixelSize(pixelSize);
    return f;
}
inline QString iconString(int codePoint) { return glyph(codePoint); }

namespace Glyph {
constexpr int More = 0xE712;
constexpr int Settings = 0xE713;
constexpr int Play = 0xE768;
constexpr int Pause = 0xE769;
constexpr int Stop = 0xE71A;
constexpr int Add = 0xE710;
constexpr int Remove = 0xE738;
constexpr int Cancel = 0xE711;
constexpr int Folder = 0xE8B7;
constexpr int Refresh = 0xE72C;
constexpr int Sync = 0xE895;
constexpr int Volume = 0xE767;
constexpr int Undo = 0xE7A7;
constexpr int Link = 0xE71B;
constexpr int Edit = 0xE70F;
constexpr int Accept = 0xE73E;
constexpr int Copy = 0xE8C8;
constexpr int Note = 0xE70B;
constexpr int ZoomIn = 0xE8A3;
constexpr int ZoomOut = 0xE71F;
constexpr int Picture = 0xEB9F;
} // namespace Glyph

namespace Icon = Glyph;

} // namespace Theme
