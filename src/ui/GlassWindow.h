#pragma once

#include <QColor>

class QWidget;

// Win11 backdrop blur for frameless desktop widgets.
// Uses acrylic-behind through SetWindowCompositionAttribute (it keeps blurring
// while the window is inactive, unlike Mica) and DWM attributes for corners.
namespace GlassWindow {

// Returns true if the real blur was enabled.
bool apply(QWidget* widget, bool glass, const QColor& tint, int tintAlpha);

} // namespace GlassWindow
