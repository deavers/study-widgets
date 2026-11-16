#pragma once

#include "Theme.h"
#include "ui/GlassWindow.h"
#include "ui/WidgetChrome.h"

#include <QAction>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QLayout>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <cmath>
#include <functional>
#include <utility>

// Base class for every desktop widget: frameless glass card with a macOS-style
// header, drag / resize / scale / compact support and persisted preferences.
//
// Widgets put their content into bodyLayout() and never touch the window frame.
class WidgetBase : public QWidget
{
public:
    explicit WidgetBase(const QString& widgetId, QWidget* parent = nullptr)
        : QWidget(parent),
          m_widgetId(widgetId)
    {
        loadWindowPrefs();
        applyWindowMode();
        setMouseTracking(true);

        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(12, 12, 12, 12);
        m_rootLayout->setSpacing(6);

        m_chrome = new WidgetChrome(this);
        m_chrome->lights->onClose = [this]() { hideToTray(); };
        m_chrome->lights->onCompact = [this]() { toggleCompact(); };
        m_chrome->lights->onZoom = [this]() { cycleScale(); };
        connect(m_chrome->menuButton, &QPushButton::clicked, this, [this]() {
            showSettingsMenu(m_chrome->menuButton->mapToGlobal(QPoint(0, m_chrome->menuButton->height())));
        });
        m_chrome->hide(); // Pure frameless widget style: no header bar, all controls via Right-Click (ПКМ)
        setChromeTitle("~/" + widgetId + " $", QString());

        m_compactFallbackLabel = new QLabel(this);
        m_compactFallbackLabel->setProperty("role", "mono");
        m_compactFallbackLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_compactFallbackLabel->hide();
        m_rootLayout->addWidget(m_compactFallbackLabel);

        m_body = new QWidget(this);
        m_bodyLayout = new QVBoxLayout(m_body);
        m_bodyLayout->setContentsMargins(0, 0, 0, 0);
        m_bodyLayout->setSpacing(6);
        m_rootLayout->addWidget(m_body, 1);

        m_savePosTimer = new QTimer(this);
        m_savePosTimer->setSingleShot(true);
        m_savePosTimer->setInterval(350);
        connect(m_savePosTimer, &QTimer::timeout, this, [this]() { savePosition(); });

        connect(&Theme::notifier(), &Theme::Notifier::changed, this, [this]() { refreshTheme(); });

        setStyleSheet(Theme::styleSheet(m_scaleFactor));
        restorePosition();
    }

    void setHideHandler(std::function<void()> handler) { m_hideHandler = std::move(handler); }
    void setExitHandler(std::function<void()> handler) { m_exitHandler = std::move(handler); }

    QString widgetId() const { return m_widgetId; }

    // Size at 100% scale in normal mode.
    void setBaseWidgetSize(int width, int height)
    {
        m_baseSize = QSize(width, height);
        applyScaledFixedSize();
    }

    // Optional: a compact variant (shown instead of the body in compact mode).
    void setCompactWidget(QWidget* widget, int bodyHeight)
    {
        m_compactWidget = widget;
        m_compactBodyHeight = bodyHeight;
        m_rootLayout->addWidget(widget);
        widget->hide();
    }

    bool isCompact() const { return m_compact; }

    void setCompact(bool compact)
    {
        if (m_compact == compact) {
            return;
        }
        m_compact = compact;
        saveWindowPrefs();
        m_body->setVisible(!compact);
        if (m_compactWidget) {
            m_compactWidget->setVisible(compact);
        } else if (m_compactFallbackLabel) {
            m_compactFallbackLabel->setVisible(compact);
        }
        applyScaledFixedSize();
        onCompactChanged(compact);
        update();
    }

    void toggleCompact() { setCompact(!m_compact); }

    virtual void setScaleFactor(double factor)
    {
        m_scaleFactor = qBound(0.65, factor, 1.75);
        m_customSize = QSize();
        saveWindowPrefs();
        applyScaledFixedSize();
        setStyleSheet(Theme::styleSheet(m_scaleFactor));
    }

    double scaleFactor() const { return m_scaleFactor; }

    QSize baseWidgetSize() const { return m_baseSize; }

protected:
    QVBoxLayout* bodyLayout() const { return m_bodyLayout; }
    WidgetChrome* chrome() const { return m_chrome; }

    void setRootMargins(int left, int top, int right, int bottom)
    {
        m_rootLayout->setContentsMargins(left, top, right, bottom);
    }

    void setChromeTitle(const QString& path, const QString& title)
    {
        m_chromePath = path;
        m_chromeTitle = title;
        m_chrome->setPrompt(path, title);
        if (m_compactFallbackLabel) {
            m_compactFallbackLabel->setText(
                QString("%1 <span style=\"color:%2\">%3</span>")
                    .arg(path.toHtmlEscaped(), Theme::accentHex(), title.toHtmlEscaped()));
            m_compactFallbackLabel->setTextFormat(Qt::RichText);
        }
    }

    virtual void buildContextMenu(QMenu* menu) { Q_UNUSED(menu); }

    virtual void onCompactChanged(bool compact) { Q_UNUSED(compact); }

    virtual void onThemeChanged() {}

    void refreshTheme()
    {
        setStyleSheet(Theme::styleSheet(m_scaleFactor));
        setChromeTitle(m_chromePath, m_chromeTitle);
        applyGlass();
        onThemeChanged();
        update();
    }

    void hideToTray()
    {
        hide();
        if (m_hideHandler) {
            m_hideHandler();
        }
    }

    void requestApplicationExit()
    {
        if (m_exitHandler) {
            m_exitHandler();
        }
    }

    void cycleScale()
    {
        static const double steps[] = {0.85, 1.0, 1.15, 1.3};
        int next = 0;
        for (int i = 0; i < 4; ++i) {
            if (m_scaleFactor >= steps[i] - 0.02) {
                next = (i + 1) % 4;
            }
        }
        setScaleFactor(steps[next]);
    }

    int glassTintAlpha() const
    {
        // Glass: light tint on top of the blur. Soft: nearly opaque.
        if (Theme::glassMode() == Theme::GlassMode::Glass && m_glassActive) {
            return static_cast<int>(70 + m_density * 110);
        }
        return static_cast<int>(150 + m_density * 90);
    }

    void applyGlass()
    {
        if (!isVisible() && !testAttribute(Qt::WA_WState_Created)) {
            return;
        }
        const bool wantGlass = Theme::glassMode() == Theme::GlassMode::Glass;
        m_glassActive = GlassWindow::apply(this, wantGlass, Theme::windowTint(),
                                           wantGlass ? static_cast<int>(40 + m_density * 90) : 0);
        if (wantGlass && !m_glassActive) {
            m_glassActive = false; // graceful fallback to Soft painting
        }
        update();
    }

    void showEvent(QShowEvent* event) override
    {
        if (layout()) {
            layout()->setSizeConstraint(QLayout::SetNoConstraint);
        }
        applyScaledFixedSize();
        QWidget::showEvent(event);
        applyGlass();
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const bool glass = Theme::glassMode() == Theme::GlassMode::Glass && m_glassActive;
        const qreal radius = glass ? 9.0 : 18.0;
        const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        QColor tint = Theme::windowTint();
        tint.setAlpha(glassTintAlpha());
        painter.setPen(Qt::NoPen);
        painter.setBrush(tint);
        painter.drawRoundedRect(card, radius, radius);

        // Accent aurora in the top-left corner (hacker hint), very subtle.
        QRadialGradient aurora(card.topLeft() + QPointF(40, 10), card.width() * 0.8);
        QColor a = Theme::accentColor();
        a.setAlpha(m_locked ? 10 : 26);
        aurora.setColorAt(0.0, a);
        a.setAlpha(0);
        aurora.setColorAt(1.0, a);
        painter.setBrush(aurora);
        painter.drawRoundedRect(card, radius, radius);

        // Hairline border + top highlight (Apple glass edge).
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, m_locked ? 22 : 40), 1.0));
        painter.drawRoundedRect(card, radius, radius);

        QLinearGradient top(card.left() + radius, card.top(), card.right() - radius, card.top());
        top.setColorAt(0.0, QColor(255, 255, 255, 0));
        top.setColorAt(0.5, QColor(255, 255, 255, 70));
        top.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(QPen(QBrush(top), 1.0));
        painter.drawLine(QPointF(card.left() + radius, card.top() + 0.5),
                         QPointF(card.right() - radius, card.top() + 0.5));

        if (!m_locked && !m_compact) {
            painter.setPen(QPen(QColor(255, 255, 255, 60), 1.2));
            const qreal rx = card.right() - 6;
            const qreal ry = card.bottom() - 6;
            painter.drawLine(QPointF(rx - 7, ry), QPointF(rx, ry - 7));
            painter.drawLine(QPointF(rx - 3, ry), QPointF(rx, ry - 3));
        }
    }

    void showSettingsMenu(const QPoint& globalPos)
    {
        QMenu menu(this);
        menu.setStyleSheet(Theme::styleSheet(1.0) + R"(
            QMenu { background: #10151F; color: #F2F5FA; border: 1px solid rgba(255,255,255,45);
                    border-radius: 10px; padding: 6px; font-size: 12px; }
            QMenu::item { padding: 6px 18px; border-radius: 6px; }
            QMenu::item:selected { background: rgba(196,181,253,38); color: #FFFFFF; }
            QMenu::separator { height: 1px; background: rgba(255,255,255,28); margin: 5px 8px; }
        )");

        buildContextMenu(&menu);
        if (!menu.actions().isEmpty()) {
            menu.addSeparator();
        }

        QAction* compactAction = menu.addAction(QString::fromUtf8("Компактный режим (свернуть)"));
        compactAction->setCheckable(true);
        compactAction->setChecked(m_compact);

        QMenu* scaleMenu = menu.addMenu(QString::fromUtf8("Масштаб (Ctrl+колёсико)"));
        QAction* sc75 = scaleMenu->addAction("75%");
        QAction* sc85 = scaleMenu->addAction("85%");
        QAction* sc100 = scaleMenu->addAction("100% (стандарт)");
        QAction* sc115 = scaleMenu->addAction("115%");
        QAction* sc130 = scaleMenu->addAction("130%");
        scaleMenu->addSeparator();
        QAction* resetSizeAct = scaleMenu->addAction(QString::fromUtf8("Сбросить размер по умолчанию"));

        QMenu* styleMenu = menu.addMenu(QString::fromUtf8("Стиль и стекло"));
        QAction* d55 = styleMenu->addAction(QString::fromUtf8("Стекло: Прозрачное"));
        QAction* d75 = styleMenu->addAction(QString::fromUtf8("Стекло: Лёгкое"));
        QAction* d100 = styleMenu->addAction(QString::fromUtf8("Стекло: Плотное"));
        styleMenu->addSeparator();
        QAction* accCyan = styleMenu->addAction(QString::fromUtf8("Акцент: Cyan"));
        QAction* accGreen = styleMenu->addAction(QString::fromUtf8("Акцент: Green"));
        QAction* accViolet = styleMenu->addAction(QString::fromUtf8("Акцент: Violet"));
        styleMenu->addSeparator();
        QAction* toggleBlur = styleMenu->addAction(
            Theme::glassMode() == Theme::GlassMode::Glass
                ? QString::fromUtf8("Переключить в Soft (без Blur)")
                : QString::fromUtf8("Переключить в Glass (Blur)"));

        QMenu* winMenu = menu.addMenu(QString::fromUtf8("Позиция и окно"));
        QAction* desktopPinAction = winMenu->addAction(QString::fromUtf8("На рабочем столе (под окнами)"));
        desktopPinAction->setCheckable(true);
        desktopPinAction->setChecked(!m_alwaysOnTop);

        QAction* topPinAction = winMenu->addAction(QString::fromUtf8("Поверх всех окон"));
        topPinAction->setCheckable(true);
        topPinAction->setChecked(m_alwaysOnTop);

        winMenu->addSeparator();
        QAction* lockAction = winMenu->addAction(QString::fromUtf8("Заблокировать позицию и размер"));
        lockAction->setCheckable(true);
        lockAction->setChecked(m_locked);

        menu.addSeparator();
        QAction* hideAction = menu.addAction(QString::fromUtf8("Скрыть виджет"));

        QAction* chosen = menu.exec(globalPos);
        if (!chosen) {
            return;
        }

        if (chosen == compactAction) {
            setCompact(compactAction->isChecked());
        } else if (chosen == desktopPinAction) {
            m_alwaysOnTop = false;
            saveWindowPrefs();
            applyWindowMode();
        } else if (chosen == topPinAction) {
            m_alwaysOnTop = true;
            saveWindowPrefs();
            applyWindowMode();
        } else if (chosen == lockAction) {
            m_locked = lockAction->isChecked();
            saveWindowPrefs();
            update();
        } else if (chosen == sc75) {
            setScaleFactor(0.75);
        } else if (chosen == sc85) {
            setScaleFactor(0.85);
        } else if (chosen == sc100) {
            setScaleFactor(1.00);
        } else if (chosen == sc115) {
            setScaleFactor(1.15);
        } else if (chosen == sc130) {
            setScaleFactor(1.30);
        } else if (chosen == resetSizeAct) {
            m_customSize = QSize();
            setScaleFactor(1.00);
        } else if (chosen == d55) {
            setDensity(0.35);
        } else if (chosen == d75) {
            setDensity(0.65);
        } else if (chosen == d100) {
            setDensity(1.0);
        } else if (chosen == accCyan) {
            Theme::setAccent(Theme::Accent::Cyan);
        } else if (chosen == accGreen) {
            Theme::setAccent(Theme::Accent::Green);
        } else if (chosen == accViolet) {
            Theme::setAccent(Theme::Accent::Violet);
        } else if (chosen == toggleBlur) {
            Theme::setGlassMode(Theme::glassMode() == Theme::GlassMode::Glass
                                    ? Theme::GlassMode::Soft
                                    : Theme::GlassMode::Glass);
        } else if (chosen == hideAction) {
            hideToTray();
        }
    }

    void setDensity(double density)
    {
        m_density = qBound(0.0, density, 1.0);
        saveWindowPrefs();
        applyGlass();
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        showSettingsMenu(event->globalPos());
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (!m_locked && (event->modifiers() & Qt::ControlModifier)) {
            const int delta = event->angleDelta().y();
            if (delta != 0) {
                setScaleFactor(m_scaleFactor + (delta > 0 ? 0.05 : -0.05));
                event->accept();
                return;
            }
        }
        QWidget::wheelEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && (m_compact || event->position().y() < 26)) {
            toggleCompact();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && !m_locked) {
            if (!m_compact && isInResizeCorner(event->position().toPoint())) {
                m_isResizing = true;
                m_resizeStartGlobal = event->globalPosition().toPoint();
                m_resizeStartSize = size();
                event->accept();
                return;
            }

            if (QWindow* win = windowHandle()) {
                if (win->startSystemMove()) {
                    event->accept();
                    return;
                }
            }
            m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
            m_isDragging = true;
            event->accept();
            return;
        }

        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!m_locked) {
            if (m_isResizing && (event->buttons() & Qt::LeftButton)) {
                const QPoint delta = event->globalPosition().toPoint() - m_resizeStartGlobal;
                const int newW = qBound(220, m_resizeStartSize.width() + delta.x(), 1600);
                const int newH = qBound(130, m_resizeStartSize.height() + delta.y(), 1200);
                m_customSize = QSize(newW, newH);
                setFixedSize(m_customSize);
                event->accept();
                return;
            }

            if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
                move(event->globalPosition().toPoint() - m_dragOffset);
                event->accept();
                return;
            }

            if (!m_compact && isInResizeCorner(event->position().toPoint())) {
                setCursor(Qt::SizeFDiagCursor);
            } else {
                unsetCursor();
            }
        }

        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && (m_isDragging || m_isResizing)) {
            m_isDragging = false;
            m_isResizing = false;
            saveWindowPrefs();
            savePosition();
            event->accept();
            return;
        }

        QWidget::mouseReleaseEvent(event);
    }

    void moveEvent(QMoveEvent* event) override
    {
        QWidget::moveEvent(event);
        if (m_savePosTimer) {
            m_savePosTimer->start();
        }
    }

    void closeEvent(QCloseEvent* event) override
    {
        savePosition();
        event->accept();
    }

private:
    bool isInResizeCorner(const QPoint& pt) const
    {
        return pt.x() >= width() - 16 && pt.y() >= height() - 16;
    }

    void applyScaledFixedSize()
    {
        if (layout()) {
            layout()->setSizeConstraint(QLayout::SetNoConstraint);
        }

        if (m_compact) {
            const int w = static_cast<int>(std::round(m_baseSize.width() * m_scaleFactor));
            const int bodyH = m_compactWidget ? m_compactBodyHeight : 20;
            const int h = static_cast<int>(std::round((24 + bodyH) * qMax(1.0, m_scaleFactor)));
            setFixedSize(qMax(200, w), h);
            return;
        }

        if (m_customSize.isValid() && m_customSize.width() >= 200 && m_customSize.height() >= 120) {
            const QSize safeSize = m_customSize.expandedTo(minimumSizeHint());
            setFixedSize(safeSize);
            return;
        }
        if (!m_baseSize.isValid() || m_baseSize.width() <= 0 || m_baseSize.height() <= 0) {
            return;
        }
        const QSize target(static_cast<int>(std::round(m_baseSize.width() * m_scaleFactor)),
                           static_cast<int>(std::round(m_baseSize.height() * m_scaleFactor)));
        setFixedSize(target.expandedTo(minimumSizeHint()));
    }

    void applyWindowMode()
    {
        const bool wasVisible = isVisible();
        Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::Tool;
        flags |= m_alwaysOnTop ? Qt::WindowStaysOnTopHint : Qt::WindowStaysOnBottomHint;
        setWindowFlags(flags);
        setAttribute(Qt::WA_TranslucentBackground);
        if (wasVisible) {
            show();
        }
    }

    void loadWindowPrefs()
    {
        QSettings settings;
        const QString key = "widgets/" + m_widgetId + "/";
        if (!settings.value(key + "cleanStyleV6", false).toBool()) {
            settings.remove(key + "customSize");
            settings.setValue(key + "scaleFactor", 1.0);
            settings.setValue(key + "cleanStyleV6", true);
        }
        m_alwaysOnTop = settings.value(key + "alwaysOnTop", false).toBool();
        m_locked = settings.value(key + "locked", false).toBool();
        m_density = settings.value(key + "density", 0.65).toDouble();
        m_scaleFactor = settings.value(key + "scaleFactor", 1.0).toDouble();
        m_customSize = settings.value(key + "customSize", QSize()).toSize();
        m_compact = false; // always start expanded; compact is a transient shade state
    }

    void saveWindowPrefs() const
    {
        QSettings settings;
        const QString key = "widgets/" + m_widgetId + "/";
        settings.setValue(key + "alwaysOnTop", m_alwaysOnTop);
        settings.setValue(key + "locked", m_locked);
        settings.setValue(key + "density", m_density);
        settings.setValue(key + "scaleFactor", m_scaleFactor);
        settings.setValue(key + "customSize", m_customSize);
    }

    void savePosition() const
    {
        QSettings settings;
        settings.setValue("widgets/" + m_widgetId + "/position", pos());
    }

    void restorePosition()
    {
        QSettings settings;
        QVariant saved = settings.value("widgets/" + m_widgetId + "/position");
        if (!saved.isValid()) {
            saved = settings.value("widgets/" + m_widgetId + "/pos");
        }
        if (!saved.isValid()) {
            return;
        }

        const QPoint pt = saved.toPoint();
        for (QScreen* screen : QGuiApplication::screens()) {
            if (screen && screen->availableGeometry().adjusted(-120, -60, 120, 60).contains(pt)) {
                move(pt);
                return;
            }
        }
    }

    QString m_widgetId;
    QString m_chromePath;
    QString m_chromeTitle;

    QVBoxLayout* m_rootLayout = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
    QWidget* m_body = nullptr;
    WidgetChrome* m_chrome = nullptr;
    QLabel* m_compactFallbackLabel = nullptr;
    QWidget* m_compactWidget = nullptr;
    int m_compactBodyHeight = 0;

    QSize m_baseSize;
    QSize m_customSize;
    QPoint m_dragOffset;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
    QTimer* m_savePosTimer = nullptr;

    bool m_isDragging = false;
    bool m_isResizing = false;
    bool m_alwaysOnTop = false;
    bool m_locked = false;
    bool m_compact = false;
    bool m_glassActive = false;
    double m_density = 0.65;
    double m_scaleFactor = 1.0;

    std::function<void()> m_hideHandler;
    std::function<void()> m_exitHandler;
};
