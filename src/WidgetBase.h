#pragma once

#include <QCloseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QWidget>

#include <functional>
#include <utility>

class WidgetBase : public QWidget {
public:
    explicit WidgetBase(const QString& widgetId, QWidget* parent = nullptr)
        : QWidget(parent),
          m_widgetId(widgetId) {
        setWindowFlags(
            Qt::FramelessWindowHint |
            Qt::Tool |
            Qt::WindowStaysOnTopHint
        );

        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);

        restorePosition();
    }

    void setHideHandler(std::function<void()> handler) {
        m_hideHandler = std::move(handler);
    }

protected:
    void hideToTray() {
        hide();

        if (m_hideHandler) {
            m_hideHandler();
        }
    }

    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(28, 30, 36, 242));

        const QRect cardRect = rect().adjusted(1, 1, -1, -1);
        painter.drawRoundedRect(cardRect, 16, 16);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragOffset = globalMousePosition(event) - frameGeometry().topLeft();
            m_isDragging = true;
            event->accept();
            return;
        }

        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
            move(globalMousePosition(event) - m_dragOffset);
            event->accept();
            return;
        }

        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_isDragging) {
            m_isDragging = false;
            savePosition();
            event->accept();
            return;
        }

        QWidget::mouseReleaseEvent(event);
    }

    void closeEvent(QCloseEvent* event) override {
        savePosition();
        event->accept();
    }

private:
    static QPoint globalMousePosition(const QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return event->globalPosition().toPoint();
#else
        return event->globalPos();
#endif
    }

    void savePosition() const {
        QSettings settings;
        settings.setValue("widgets/" + m_widgetId + "/position", pos());
    }

    void restorePosition() {
        QSettings settings;

        const QVariant savedPosition =
            settings.value("widgets/" + m_widgetId + "/position");

        if (savedPosition.isValid()) {
            move(savedPosition.toPoint());
        }
    }

    QString m_widgetId;
    QPoint m_dragOffset;
    bool m_isDragging = false;

    std::function<void()> m_hideHandler;
};